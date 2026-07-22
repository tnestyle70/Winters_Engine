package main

import (
	"bytes"
	"context"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"net/http"
	"os"
	"sort"
	"strings"
	"sync"
	"sync/atomic"
	"time"
)

type observation struct {
	latency time.Duration
	status  int
	err     bool
}

type report struct {
	URL            string         `json:"url"`
	Method         string         `json:"method"`
	Duration       string         `json:"duration"`
	TargetRPS      int            `json:"target_rps"`
	Concurrency    int            `json:"concurrency"`
	Expected       int64          `json:"expected"`
	Scheduled      int64          `json:"scheduled"`
	ScheduleRate   float64        `json:"schedule_rate"`
	Completed      int            `json:"completed"`
	Dropped        int64          `json:"dropped"`
	Errors         int            `json:"errors"`
	ErrorRate      float64        `json:"error_rate"`
	AchievedRPS    float64        `json:"achieved_rps"`
	LatencyP50MS   float64        `json:"latency_p50_ms"`
	LatencyP95MS   float64        `json:"latency_p95_ms"`
	LatencyP99MS   float64        `json:"latency_p99_ms"`
	LatencyMaxMS   float64        `json:"latency_max_ms"`
	StatusCounts   map[string]int `json:"status_counts"`
	ThresholdsPass bool           `json:"thresholds_pass"`
}

func percentile(values []time.Duration, quantile float64) time.Duration {
	if len(values) == 0 {
		return 0
	}
	index := int(float64(len(values)-1) * quantile)
	return values[index]
}

func milliseconds(value time.Duration) float64 {
	return float64(value.Microseconds()) / 1000
}

func main() {
	url := flag.String("url", "", "absolute HTTP endpoint")
	method := flag.String("method", http.MethodGet, "HTTP method")
	body := flag.String("body", "", "optional request body")
	duration := flag.Duration("duration", 15*time.Second, "test duration")
	targetRPS := flag.Int("rps", 50, "scheduled requests per second")
	concurrency := flag.Int("concurrency", 16, "worker count")
	requestTimeout := flag.Duration("timeout", 5*time.Second, "per-request timeout")
	maximumErrorRate := flag.Float64("max-error-rate", 0.01, "failure threshold")
	maximumP95MS := flag.Float64("max-p95-ms", 250, "p95 latency threshold")
	flag.Parse()

	if *url == "" || *duration <= 0 || *targetRPS <= 0 || *targetRPS > 100000 ||
		*concurrency <= 0 || *concurrency > 4096 || *requestTimeout <= 0 {
		fmt.Fprintln(os.Stderr, "invalid load-test arguments")
		os.Exit(2)
	}

	token := os.Getenv("LOADTEST_BEARER_TOKEN")
	requestBody := *body
	if requestBody == "" {
		requestBody = os.Getenv("LOADTEST_BODY")
	}
	transport := &http.Transport{
		MaxIdleConns:        *concurrency * 2,
		MaxIdleConnsPerHost: *concurrency,
		IdleConnTimeout:     30 * time.Second,
	}
	client := &http.Client{Transport: transport}
	defer transport.CloseIdleConnections()

	jobs := make(chan struct{}, *concurrency*2)
	observations := make(chan observation, *concurrency*4)
	latencies := make([]time.Duration, 0)
	statusCounts := make(map[string]int)
	requestErrors := 0
	var collector sync.WaitGroup
	collector.Add(1)
	go func() {
		defer collector.Done()
		for value := range observations {
			latencies = append(latencies, value.latency)
			statusCounts[fmt.Sprintf("%d", value.status)]++
			if value.err {
				requestErrors++
			}
		}
	}()

	var workers sync.WaitGroup
	for range *concurrency {
		workers.Add(1)
		go func() {
			defer workers.Done()
			for range jobs {
				ctx, cancel := context.WithTimeout(context.Background(), *requestTimeout)
				request, err := http.NewRequestWithContext(
					ctx, strings.ToUpper(*method), *url, bytes.NewBufferString(requestBody))
				if err == nil {
					request.Header.Set("Content-Type", "application/json")
					if token != "" {
						request.Header.Set("Authorization", "Bearer "+token)
					}
				}
				start := time.Now()
				status := 0
				failed := err != nil
				if err == nil {
					response, requestErr := client.Do(request)
					if requestErr != nil {
						failed = true
					} else {
						status = response.StatusCode
						_, copyErr := io.Copy(io.Discard, io.LimitReader(response.Body, 2<<20))
						closeErr := response.Body.Close()
						failed = copyErr != nil || closeErr != nil || status < 200 || status >= 300
					}
				}
				cancel()
				observations <- observation{latency: time.Since(start), status: status, err: failed}
			}
		}()
	}

	start := time.Now()
	deadline := start.Add(*duration)
	interval := time.Second / time.Duration(*targetRPS)
	ticker := time.NewTicker(interval)
	var scheduled atomic.Int64
	var dropped atomic.Int64
	for now := range ticker.C {
		if !now.Before(deadline) {
			break
		}
		scheduled.Add(1)
		select {
		case jobs <- struct{}{}:
		default:
			dropped.Add(1)
		}
	}
	ticker.Stop()
	close(jobs)
	workers.Wait()
	close(observations)
	collector.Wait()
	elapsed := time.Since(start)

	errors := requestErrors + int(dropped.Load())
	sort.Slice(latencies, func(i, j int) bool { return latencies[i] < latencies[j] })
	completed := len(latencies)
	total := completed + int(dropped.Load())
	expected := int64(*duration / interval)
	scheduleRate := 0.0
	if expected > 0 {
		scheduleRate = float64(scheduled.Load()) / float64(expected)
	}
	errorRate := 0.0
	if total > 0 {
		errorRate = float64(errors) / float64(total)
	}
	maximum := time.Duration(0)
	if completed > 0 {
		maximum = latencies[completed-1]
	}
	result := report{
		URL:          *url,
		Method:       strings.ToUpper(*method),
		Duration:     elapsed.Round(time.Millisecond).String(),
		TargetRPS:    *targetRPS,
		Concurrency:  *concurrency,
		Expected:     expected,
		Scheduled:    scheduled.Load(),
		ScheduleRate: scheduleRate,
		Completed:    completed,
		Dropped:      dropped.Load(),
		Errors:       errors,
		ErrorRate:    errorRate,
		AchievedRPS:  float64(completed) / elapsed.Seconds(),
		LatencyP50MS: milliseconds(percentile(latencies, 0.50)),
		LatencyP95MS: milliseconds(percentile(latencies, 0.95)),
		LatencyP99MS: milliseconds(percentile(latencies, 0.99)),
		LatencyMaxMS: milliseconds(maximum),
		StatusCounts: statusCounts,
		ThresholdsPass: scheduleRate >= 0.95 && errorRate <= *maximumErrorRate &&
			milliseconds(percentile(latencies, 0.95)) <= *maximumP95MS,
	}
	encoded, _ := json.Marshal(result)
	fmt.Println(string(encoded))
	if !result.ThresholdsPass {
		os.Exit(1)
	}
}

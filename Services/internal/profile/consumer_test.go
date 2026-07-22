package profile

import "testing"

func TestRewardRPForMatchResult(t *testing.T) {
	tests := []struct {
		name   string
		result string
		want   int64
	}{
		{name: "winner", result: "win", want: 1000},
		{name: "loser", result: "loss", want: 1000},
		{name: "draw", result: "draw", want: 1000},
		{name: "missing", result: "", want: 0},
		{name: "aborted", result: "aborted", want: 0},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			if got := rewardRPForMatchResult(test.result); got != test.want {
				t.Fatalf("rewardRPForMatchResult(%q) = %d, want %d", test.result, got, test.want)
			}
		})
	}
}

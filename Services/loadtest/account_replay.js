import http from "k6/http";
import { check, sleep } from "k6";

export const options = {
  scenarios: {
    account_read_path: {
      executor: "ramping-arrival-rate",
      startRate: 10,
      timeUnit: "1s",
      preAllocatedVUs: 20,
      maxVUs: 200,
      stages: [
        { target: 50, duration: "30s" },
        { target: 100, duration: "60s" },
        { target: 250, duration: "60s" },
        { target: 500, duration: "60s" },
        { target: 250, duration: "120s" },
        { target: 0, duration: "10s" },
      ],
    },
  },
  thresholds: {
    http_req_failed: ["rate<0.01"],
    http_req_duration: ["p(95)<250", "p(99)<500"],
    dropped_iterations: ["count==0"],
  },
};

const baseURL = __ENV.BASE_URL || "http://127.0.0.1";
const token = __ENV.ACCESS_TOKEN;
const headers = { Authorization: `Bearer ${token}` };

export default function () {
  const responses = http.batch([
    ["GET", `${baseURL}:8084/profile/me`, null, { headers }],
    ["GET", `${baseURL}:8086/shop/storefront`, null, { headers }],
    ["GET", `${baseURL}:8087/replay/me?limit=20`, null, { headers }],
  ]);
  check(responses, {
    "profile 200": (values) => values[0].status === 200,
    "shop 200": (values) => values[1].status === 200,
    "replay 200": (values) => values[2].status === 200,
  });
  sleep(0.1);
}

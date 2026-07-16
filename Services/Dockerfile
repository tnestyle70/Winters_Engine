FROM golang:1.26.2-alpine AS build

ARG SERVICE
WORKDIR /src/Services
COPY Services/go.mod Services/go.sum ./
RUN go mod download
COPY Services/ ./
RUN test -n "$SERVICE" && CGO_ENABLED=0 GOOS=linux go build -trimpath -ldflags="-s -w" -o /out/service "./cmd/${SERVICE}"

FROM gcr.io/distroless/static-debian12:nonroot
WORKDIR /app
COPY --from=build /out/service /app/service
COPY Data/Account/AccountEconomyPolicy.json /app/Data/Account/AccountEconomyPolicy.json
ENV WINTERS_ACCOUNT_POLICY_PATH=/app/Data/Account/AccountEconomyPolicy.json
ENTRYPOINT ["/app/service"]

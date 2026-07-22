locals {
  name = "${var.project_name}-${var.environment}"
  azs = slice(
    data.aws_availability_zones.available.names,
    0,
    var.availability_zone_count
  )

  services = {
    auth = {
      port     = 8081
      priority = 10
      paths    = ["/auth", "/auth/*", "/health"]
    }
    leaderboard = {
      port     = 8082
      priority = 20
      paths    = ["/leaderboard", "/leaderboard/*"]
    }
    matchmaking = {
      port     = 8083
      priority = 30
      paths    = ["/matchmaking", "/matchmaking/*"]
    }
    profile = {
      port     = 8084
      priority = 40
      paths    = ["/profile", "/profile/*"]
    }
    payment = {
      port     = 8085
      priority = 50
      paths    = ["/payment", "/payment/*"]
    }
    shop = {
      port     = 8086
      priority = 60
      paths    = ["/shop", "/shop/*"]
    }
    replay = {
      port     = 8087
      priority = 70
      paths    = ["/replay", "/replay/*"]
    }
  }

  image_names   = toset(concat(keys(local.services), ["migrate"]))
  kafka_brokers = var.enable_msk ? aws_msk_cluster.events[0].bootstrap_brokers_tls : var.kafka_brokers

  common_environment = [
    { name = "WINTERS_ENV", value = "production" },
    { name = "AWS_REGION", value = var.aws_region },
    { name = "DB_HOST", value = aws_db_instance.postgres.address },
    { name = "DB_PORT", value = tostring(aws_db_instance.postgres.port) },
    { name = "DB_USER", value = var.db_username },
    { name = "DB_NAME", value = var.db_name },
    { name = "DB_POOL_MAX", value = "10" },
    { name = "DB_SSL_MODE", value = "require" },
    { name = "REDIS_ADDR", value = "${aws_elasticache_replication_group.redis.primary_endpoint_address}:6379" },
    { name = "REDIS_POOL_MAX", value = "10" },
    { name = "REDIS_TLS_ENABLED", value = "true" },
    { name = "KAFKA_BROKERS", value = local.kafka_brokers },
    { name = "KAFKA_TLS_ENABLED", value = "true" },
    { name = "WINTERS_GAME_HOST", value = var.game_server_host },
    { name = "WINTERS_GAME_PORT", value = tostring(var.game_server_port) },
    { name = "WINTERS_GAME_TRANSPORT", value = "udp" },
    { name = "REPLAY_BUCKET", value = aws_s3_bucket.replays.id },
    { name = "REPLAY_OBJECT_PREFIX", value = "replays" },
    { name = "S3_ENDPOINT", value = "" },
    { name = "S3_USE_PATH_STYLE", value = "false" },
    { name = "REPLAY_RETENTION_DAYS", value = tostring(var.replay_retention_days) },
    { name = "WINTERS_ACCOUNT_POLICY_PATH", value = "/app/Data/Account/AccountEconomyPolicy.json" }
  ]

  common_secrets = [
    { name = "DB_PASSWORD", valueFrom = "${aws_db_instance.postgres.master_user_secret[0].secret_arn}:password::" },
    { name = "JWT_ACCESS_SECRET", valueFrom = "${aws_secretsmanager_secret.application.arn}:JWT_ACCESS_SECRET::" },
    { name = "JWT_REFRESH_SECRET", valueFrom = "${aws_secretsmanager_secret.application.arn}:JWT_REFRESH_SECRET::" },
    { name = "WINTERS_MATCH_TICKET_SECRET", valueFrom = "${aws_secretsmanager_secret.application.arn}:WINTERS_MATCH_TICKET_SECRET::" },
    { name = "REPLAY_INTERNAL_TOKEN", valueFrom = "${aws_secretsmanager_secret.application.arn}:REPLAY_INTERNAL_TOKEN::" }
  ]
}

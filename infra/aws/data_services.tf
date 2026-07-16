resource "aws_db_subnet_group" "postgres" {
  name       = "${local.name}-postgres"
  subnet_ids = values(aws_subnet.private)[*].id
}

resource "aws_db_instance" "postgres" {
  identifier                      = "${local.name}-postgres"
  engine                          = "postgres"
  engine_version                  = "16"
  instance_class                  = var.db_instance_class
  db_name                         = var.db_name
  username                        = var.db_username
  manage_master_user_password     = true
  allocated_storage               = var.db_allocated_storage_gib
  max_allocated_storage           = var.db_max_allocated_storage_gib
  storage_type                    = "gp3"
  storage_encrypted               = true
  multi_az                        = var.db_multi_az
  publicly_accessible             = false
  db_subnet_group_name            = aws_db_subnet_group.postgres.name
  vpc_security_group_ids          = [aws_security_group.rds.id]
  backup_retention_period         = 7
  auto_minor_version_upgrade      = true
  deletion_protection             = var.enable_deletion_protection
  skip_final_snapshot             = !var.enable_deletion_protection
  final_snapshot_identifier       = var.enable_deletion_protection ? "${local.name}-postgres-final" : null
  enabled_cloudwatch_logs_exports = ["postgresql", "upgrade"]
  apply_immediately               = var.environment == "dev"
}

resource "aws_elasticache_subnet_group" "redis" {
  name       = "${local.name}-redis"
  subnet_ids = values(aws_subnet.private)[*].id
}

resource "aws_elasticache_replication_group" "redis" {
  replication_group_id       = "${local.name}-redis"
  description                = "Winters cache and matchmaking queue"
  engine                     = "redis"
  engine_version             = "7.1"
  node_type                  = var.redis_node_type
  port                       = 6379
  num_cache_clusters         = var.redis_node_count
  automatic_failover_enabled = var.redis_node_count > 1
  multi_az_enabled           = var.redis_node_count > 1
  at_rest_encryption_enabled = true
  transit_encryption_enabled = true
  subnet_group_name          = aws_elasticache_subnet_group.redis.name
  security_group_ids         = [aws_security_group.redis.id]
  snapshot_retention_limit   = 1
  apply_immediately          = var.environment == "dev"
}

resource "aws_cloudwatch_log_group" "msk" {
  name              = "/aws/msk/${local.name}"
  retention_in_days = 14
}

resource "aws_msk_cluster" "events" {
  count = var.enable_msk ? 1 : 0

  cluster_name           = "${local.name}-events"
  kafka_version          = var.msk_kafka_version
  number_of_broker_nodes = length(local.azs)

  broker_node_group_info {
    instance_type   = var.msk_instance_type
    client_subnets  = values(aws_subnet.private)[*].id
    security_groups = [aws_security_group.msk.id]
    storage_info {
      ebs_storage_info {
        volume_size = 20
      }
    }
  }

  encryption_info {
    encryption_in_transit {
      client_broker = "TLS"
      in_cluster    = true
    }
  }

  logging_info {
    broker_logs {
      cloudwatch_logs {
        enabled   = true
        log_group = aws_cloudwatch_log_group.msk.name
      }
    }
  }
}

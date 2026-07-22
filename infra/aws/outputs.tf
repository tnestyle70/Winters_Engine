output "api_base_url" {
  value = var.certificate_arn == "" ? "http://${aws_lb.api.dns_name}" : "https://${aws_lb.api.dns_name}"
}

output "ecr_repository_urls" {
  value = { for name, repository in aws_ecr_repository.service : name => repository.repository_url }
}

output "replay_bucket" {
  value = aws_s3_bucket.replays.id
}

output "application_secret_arn" {
  value = aws_secretsmanager_secret.application.arn
}

output "replay_internal_url" {
  value = "http://replay.${aws_service_discovery_private_dns_namespace.main.name}:8087"
}

output "ecs_cluster_name" {
  value = aws_ecs_cluster.main.name
}

output "private_subnet_ids" {
  value = values(aws_subnet.private)[*].id
}

output "ecs_security_group_id" {
  value = aws_security_group.ecs.id
}

output "migration_task_definition_arn" {
  value = var.deploy_services ? aws_ecs_task_definition.migrate[0].arn : null
}

output "rds_master_secret_arn" {
  value     = aws_db_instance.postgres.master_user_secret[0].secret_arn
  sensitive = true
}

output "estimated_cost_warning" {
  value = "NAT Gateway, ALB, RDS, ElastiCache, ECS and optional MSK accrue hourly charges. Destroy dev stacks when the evidence capture is complete."
}

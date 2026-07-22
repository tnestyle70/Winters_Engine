variable "project_name" {
  description = "Lowercase name prefix used by AWS resources."
  type        = string
  default     = "winters"
}

variable "environment" {
  description = "Deployment environment name."
  type        = string
  default     = "dev"
}

variable "aws_region" {
  description = "AWS region for the backend stack."
  type        = string
  default     = "ap-northeast-2"
}

variable "vpc_cidr" {
  type    = string
  default = "10.42.0.0/16"
}

variable "availability_zone_count" {
  type    = number
  default = 2

  validation {
    condition     = var.availability_zone_count >= 2 && var.availability_zone_count <= 3
    error_message = "availability_zone_count must be 2 or 3."
  }
}

variable "image_tag" {
  description = "Immutable Git SHA or release tag deployed from ECR."
  type        = string
  default     = "bootstrap"
}

variable "deploy_services" {
  description = "Create ECS task definitions and services after images and secrets exist."
  type        = bool
  default     = false
}

variable "desired_count" {
  type    = number
  default = 1
}

variable "max_task_count" {
  type    = number
  default = 3
}

variable "db_name" {
  type    = string
  default = "winters"
}

variable "db_username" {
  type    = string
  default = "winters_admin"
}

variable "db_instance_class" {
  type    = string
  default = "db.t4g.micro"
}

variable "db_allocated_storage_gib" {
  type    = number
  default = 20
}

variable "db_max_allocated_storage_gib" {
  type    = number
  default = 100
}

variable "db_multi_az" {
  type    = bool
  default = false
}

variable "enable_deletion_protection" {
  type    = bool
  default = false
}

variable "redis_node_type" {
  type    = string
  default = "cache.t4g.micro"
}

variable "redis_node_count" {
  type    = number
  default = 1
}

variable "enable_msk" {
  description = "Provision a private TLS MSK cluster. This is intentionally opt-in because it dominates dev cost."
  type        = bool
  default     = false
}

variable "kafka_brokers" {
  description = "Comma-separated TLS brokers when enable_msk=false. Required before deploy_services=true."
  type        = string
  default     = ""
}

variable "msk_kafka_version" {
  type    = string
  default = "3.7.x"
}

variable "msk_instance_type" {
  type    = string
  default = "kafka.t3.small"
}

variable "game_server_host" {
  description = "Public DNS/IP allocated by the separate EC2 or GameLift game-server stack."
  type        = string
  default     = "127.0.0.1"
}

variable "game_server_port" {
  type    = number
  default = 9000
}

variable "certificate_arn" {
  description = "Optional ACM certificate. Empty keeps the dev ALB on HTTP."
  type        = string
  default     = ""
}

variable "replay_retention_days" {
  type    = number
  default = 30
}

variable "monthly_budget_usd" {
  type    = number
  default = 100
}

variable "alert_email" {
  description = "Optional email for SNS alarms and AWS Budget notifications."
  type        = string
  default     = ""
}

variable "tags" {
  type    = map(string)
  default = {}
}

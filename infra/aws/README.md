# Winters AWS backend stack

This Terraform stack turns the local Go services into a reproducible AWS staging environment. It intentionally does not deploy the Windows IOCP game server as if it were a stateless web task; that process remains a separate EC2 or GameLift concern.

## Resources

- Public ALB; ECS Fargate tasks remain in private subnets.
- RDS PostgreSQL with an AWS-managed master password and private access.
- ElastiCache Redis with in-transit and at-rest encryption.
- Private, versioned S3 replay bucket with Block Public Access, default encryption, retention, and incomplete multipart cleanup.
- ECR repositories for seven services plus the migration job.
- Optional private TLS Amazon MSK. It is disabled by default because it is a major always-on cost.
- CloudWatch JSON logs, request-error metric filters, alarms, dashboard, SNS, and AWS Budget.
- Cloud Map private DNS for the game server to call `http://replay.<project>-<env>.internal:8087`; `/internal/*` is not routed by the public ALB.

## Required operator inputs

No AWS credentials or secret values belong in this repository. Before the first real apply, choose the region, monthly ceiling, Kafka option, domain/certificate, and game-server endpoint. Then:

1. Configure an AWS CLI profile or GitHub Actions OIDC role with least privilege.
2. Copy `terraform.tfvars.example` to an untracked `terraform.tfvars` and replace environment-specific values.
3. Initialize the remote state backend from `backend.hcl.example`.
4. Apply once with `deploy_services=false` to create infrastructure and ECR repositories.
5. Populate the output `application_secret_arn` with one JSON object containing four 32-byte-or-longer values: `JWT_ACCESS_SECRET`, `JWT_REFRESH_SECRET`, `WINTERS_MATCH_TICKET_SECRET`, and `REPLAY_INTERNAL_TOKEN`.
6. Push images tagged with an immutable Git SHA.
7. Apply with `deploy_services=true` and `desired_count=0`, run the migration task, then apply with the desired count set to at least one. The manual deploy workflow automates this order.

The RDS password is generated and owned by RDS/Secrets Manager; Terraform never receives it as a variable. The application secret container is created without a secret version so plaintext application secrets are not written to Terraform state.

## Validation

```text
terraform fmt -check -recursive
terraform init -backend=false
terraform validate
terraform plan -var-file=terraform.tfvars
```

Before `apply`, review that ECS/RDS/Redis/MSK have no public IP or public endpoint, the S3 bucket remains private, and the estimated monthly cost fits the approved ceiling.

## Cost boundary

NAT Gateway, ALB, RDS, ElastiCache, ECS, and especially MSK accrue hourly charges. This is a short-lived portfolio staging stack, not a free-tier promise. Capture evidence and run `terraform destroy` when the session is complete. A production environment should enable deletion protection, Multi-AZ RDS/Redis, HTTPS, and a remote-state backup policy.

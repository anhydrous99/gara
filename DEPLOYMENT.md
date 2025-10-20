# Deployment Guide

## Prerequisites

**System packages**:
```bash
# macOS
brew install openssl zlib cmake pkg-config glib vips

# Ubuntu
sudo apt-get install -y libssl-dev zlib1g-dev cmake build-essential \
    pkg-config libglib2.0-dev libvips-dev
```

**AWS setup**:
```bash
aws configure
aws s3 mb s3://gara-images --region us-east-1
```

## Local Development

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j
export S3_BUCKET_NAME=gara-images AWS_REGION=us-east-1 PORT=8080
./gara
```

## AWS EC2

### IAM Policy

```json
{
  "Version": "2012-10-17",
  "Statement": [{
    "Effect": "Allow",
    "Action": ["s3:PutObject", "s3:GetObject", "s3:DeleteObject", "s3:HeadObject"],
    "Resource": "arn:aws:s3:::gara-images/*"
  }, {
    "Effect": "Allow",
    "Action": ["s3:ListBucket"],
    "Resource": "arn:aws:s3:::gara-images"
  }]
}
```

### Setup Script

```bash
#!/bin/bash
set -e

# Install dependencies
sudo apt-get update && sudo apt-get install -y \
    libssl-dev zlib1g-dev cmake build-essential git \
    pkg-config libglib2.0-dev libvips-dev nginx

# Clone and build
cd /opt
sudo git clone https://github.com/yourusername/gara.git
cd gara/build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
strip gara

# Create systemd service
sudo tee /etc/systemd/system/gara.service > /dev/null <<EOF
[Unit]
Description=Gara Image Service
After=network.target

[Service]
Type=simple
User=ubuntu
WorkingDirectory=/opt/gara/build
Environment="S3_BUCKET_NAME=gara-images"
Environment="AWS_REGION=us-east-1"
Environment="PORT=8080"
ExecStart=/opt/gara/build/gara
Restart=always

[Install]
WantedBy=multi-user.target
EOF

# Start service
sudo systemctl daemon-reload
sudo systemctl enable --now gara
```

### Nginx Reverse Proxy

```nginx
server {
    listen 80;
    server_name your-domain.com;
    client_max_body_size 100M;

    location / {
        proxy_pass http://localhost:8080;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
    }
}
```

## Docker

```dockerfile
FROM ubuntu:22.04 AS builder
RUN apt-get update && apt-get install -y cmake build-essential \
    libssl-dev zlib1g-dev meson ninja-build pkg-config libglib2.0-dev
COPY . /app
RUN cd /app/build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j && strip gara

FROM scratch
COPY --from=builder /app/build/gara /gara
ENV S3_BUCKET_NAME=gara-images AWS_REGION=us-east-1 PORT=80
EXPOSE 80
ENTRYPOINT ["/gara"]
```

**Run**:
```bash
docker build -t gara .
docker run -d -p 80:80 \
  -e S3_BUCKET_NAME=gara-images \
  -e AWS_REGION=us-east-1 \
  -e AWS_ACCESS_KEY_ID=xxx \
  -e AWS_SECRET_ACCESS_KEY=yyy \
  gara
```

## AWS ECS/Fargate

**Task definition**:
```json
{
  "family": "gara",
  "networkMode": "awsvpc",
  "requiresCompatibilities": ["FARGATE"],
  "cpu": "1024",
  "memory": "2048",
  "taskRoleArn": "arn:aws:iam::ACCOUNT:role/gara-task-role",
  "containerDefinitions": [{
    "name": "gara",
    "image": "your-ecr/gara:latest",
    "portMappings": [{"containerPort": 80}],
    "environment": [
      {"name": "S3_BUCKET_NAME", "value": "gara-images"},
      {"name": "AWS_REGION", "value": "us-east-1"}
    ]
  }]
}
```

## Performance Tuning

```bash
# Use tmpfs for temp files
sudo mount -t tmpfs -o size=2G tmpfs /tmp/uploads

# Enable S3 transfer acceleration
aws s3api put-bucket-accelerate-configuration \
  --bucket gara-images --accelerate-configuration Status=Enabled

# Add CloudFront for CDN caching
```

## Monitoring

```bash
# Health check cron
*/5 * * * * curl -f http://localhost:8080/api/images/health || systemctl restart gara

# CloudWatch logs
aws logs tail /gara/image-service --follow
```

## Security Checklist

- [ ] S3 bucket not publicly accessible
- [ ] AWS credentials rotated regularly
- [ ] HTTPS enabled (nginx + Let's Encrypt)
- [ ] Rate limiting configured
- [ ] File size/type validation enabled
- [ ] Security groups properly configured

REGISTRY=wyga/config-service
all: build publish
build:
	GOOS=linux GOARCH=amd64 CGO_ENABLED=0 go build -ldflags="-s -w" -o binary/config-service.linux.amd64
	GOOS=linux GOARCH=arm64 CGO_ENABLED=0 go build -ldflags="-s -w" -o binary/config-service.linux.arm64
	upx binary/config-service.linux.amd64
	upx binary/config-service.linux.arm64
publish:
	docker buildx build --platform linux/amd64,linux/arm64 -t $(REGISTRY):1.0.0 $(REGISTRY):latest --no-cache --push .

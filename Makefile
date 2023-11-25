REGISTRY=wyga/configus
PLATFORMS = linux/amd64,linux/arm64/v8,linux/386,linux/arm/v7,linux/arm/v6
BUILDX = docker buildx build --provenance false --platform $(PLATFORMS)
OUTPUT = --output type=image,push=true,compression=gzip,compression-level=9,force-compression=true
TAG=1.6.1

all:
	$(BUILDX) $(OUTPUT) -t $(REGISTRY):$(TAG) -t $(REGISTRY):latest --pull .

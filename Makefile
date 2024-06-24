REGISTRY = wyga/configus
PLATFORMS = linux/amd64,linux/arm64/v8
BUILDX = docker buildx build --provenance false --platform $(PLATFORMS) --builder multiplatform-builder
OUTPUT = --output type=image,push=true,compression=gzip,compression-level=9,force-compression=true
TAG = 1.7.2

all:
	$(BUILDX) $(OUTPUT) -t $(REGISTRY):$(TAG) -t $(REGISTRY):latest --pull .
#	$(BUILDX) $(OUTPUT) -t $(REGISTRY):DEBUG -f Dockerfile.debug --pull .

em-docker:
	docker run \
	-it --rm \
	--workdir /src \
	-v .:/src \
	-u $(id -u):$(id -g) \
	emscripten/emsdk \
	/bin/bash

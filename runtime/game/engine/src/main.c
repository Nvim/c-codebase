#include "engine.h"

#include <core/log.h>

int main(void) {

	int exit_status = 0;

	if (!engine_init()) {
		LOG_FATAL("couldn't initialize engine");
		return 1;
	}

	if (!engine_run()) {
		LOG_ERR("engine main loop exited with error");
		exit_status = 2;
	}

	if (!engine_shutdown()) {
		LOG_FATAL("couldn't shutdown engine");
		return 1;
	}

	return exit_status;
}

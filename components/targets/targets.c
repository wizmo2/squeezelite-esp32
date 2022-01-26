#include "string.h"
#include "targets.h"

const struct target_s *target_set[] = { &target_muse, NULL };

void target_init(char *target) { 
	for (int i = 0; *target && target_set[i]; i++) if (strcasestr(target_set[i]->model, target)) {
		target_set[i]->init();
		break;
	}	
}

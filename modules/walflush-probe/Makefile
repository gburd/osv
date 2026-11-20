# Copyright (C) 2026 Greg Burd. BSD-3-Clause.
module: probe.so usr.manifest
probe.so: probe.c
	$(CC) -O2 -fPIC -shared -pthread -o $@ $<
usr.manifest:
	echo '/probe.so: $${MODULE_DIR}/probe.so' > $@
clean:
	rm -f probe.so usr.manifest

# Copyright (C) 2024 OSv Contributors
# This work is open source software, licensed under the terms of the
# BSD license as described in the LICENSE file in the top-level directory.

module: crucible-snapshot

crucible-snapshot: crucible-snapshot.o
	$(CXX) -o $@ $^ $(LDFLAGS)

crucible-snapshot.o: crucible-snapshot.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f crucible-snapshot *.o

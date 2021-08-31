.PHONY: all
all: prod debug
	@echo * Done building

.PHONY: prod
prod:
	gcc -O3 -o fsg fsg.c
	@echo * Production build complete

.PHONY: debug
debug:
	gcc -ggdb3 fsg.c -Wall -o fsg_dbg
	@echo * Debug build complete

.PHONY: clean
clean:
	-rm fsg fsg_dbg strings.out fsg.out
	@echo * Finished cleaning older builds

.PHONY: test
test:
	@sh -c 'echo - Removing older result files'
	@-rm strings.out fsg.out strings_filter.out fsg_filter.out

	@sh -c "echo ======= Running simple strings dump test ======="

	@sh -c "echo - Clearing cache"
	@sudo sync; sudo echo 3 > /proc/sys/vm/drop_caches;
	@sleep 10

	@sh -c "echo - Running common 'strings'"
	time -p sh -c "(strings test.iso > strings.out)"

	@sh -c "echo - Clearing cache again"
	@sudo sync; sudo echo 3 > /proc/sys/vm/drop_caches;
	@sleep 10

	@sh -c "echo - Running fsg"
	time -p sh -c "( ./fsg test.iso -q -o fsg.out )"

	@sh -c "echo - Comparing results of simple string dump test"
	@sha256sum strings.out
	@sha256sum fsg.out
	
	@sh -c "echo ======= Running simple filtering test ======="

	@sh -c "echo - Clearing cache"
	@sudo sync; sudo echo 3 > /proc/sys/vm/drop_caches;
	@sleep 10

	@sh -c "echo - Running common 'strings' and two 'grep' pipes"
	time -p sh -c "(strings test.iso | grep Microsoft | grep -v Corporation > strings_filter.out)"

	@sh -c "echo - Clearing cache again"
	@sudo sync; sudo echo 3 > /proc/sys/vm/drop_caches;
	@sleep 10

	@sh -c "echo - Running fsg"
	time -p sh -c "( ./fsg test.iso -q -f '+Microsoft -Corporation' -o fsg_filter.out )"

	@sh -c "echo - Comparing results of simple filtering test"
	@sha256sum strings_filter.out
	@sha256sum fsg_filter.out

	@sh -c "echo DONE"


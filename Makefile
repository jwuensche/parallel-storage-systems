default: task

task:
	@cp group.txt $(task)/
	@make -C $(task) clean
	@cp -r $(task) WagnerMuellerShahWuensche
	@tar czf WagnerMuellerShahWuensche.tar.gz WagnerMuellerShahWuensche
	@rm $(task)/group.txt
	@rm -r WagnerMuellerShahWuensche

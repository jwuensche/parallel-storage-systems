default: task

task:
	@cp group.txt $(task)/
	@make -C $(task) clean
	@tar czf WagnerMuellerShahWuensche.tar.gz $(task)
	@rm $(task)/group.txt

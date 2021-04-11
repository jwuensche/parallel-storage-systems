default: task

task:
	@cp group.txt $(task)/
	@tar czf WagnerMuellerNameWuensche.tar.gz $(task)
	@rm $(task)/group.txt

#source for similar code https://code.google.com/r/santanar78-/source/browse/git-instaweb.sh?name=v1.7.6.2
#!/bin/bash
DEBUG=true
if $DEBUG;
then
	valgrind --tool=memcheck --leak-check=full ./mpdweb
else
	./mpdweb
fi

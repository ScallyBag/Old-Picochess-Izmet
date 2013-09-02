To do an android compile of picochess:

1. ndk-build on the Stockfish/src directory (create a symlink to the current directory with the name 'jni').
2. Change hashtable size under dgt.cpp to 128MB to support most Android devices
3. Change book folder to ./books
4. Create an engines folder.
5. Copy over stockfish-arm to the engines folder
6. Copy over the book folder here.
7. Use python-for-android to take it thru the rest of the way!

CC = gcc
CFLAGS = -ggdb -Wall -O0
CPPFLAGS = -I/usr/include -I/usr/local/include -I/usr/local/include/oss_c_sdk -I/usr/include/apr-1
LDFLAGS = -L/usr/lib -L/usr/local/lib
LDLIBS = -loss_c_sdk -lmxml -lcurl -lapr-1 -laprutil-1 -lpthread

PROG = append resume multipart multipart-init multipart-upload multipart-list \
				multipart-complete multipart-abort multipart-upload-part
TEMPFILES = core core.* *.o temp.* *.out *~ *.exe *.stackdump

all: $(PROG)

append: append.o myoss.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

resume: resume.o myoss.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

multipart: multipart.o myoss.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

multipart-init: multipart-init.o myoss.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

multipart-upload: multipart-upload.o myoss.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

multipart-list: multipart-list.o myoss.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

multipart-complete: multipart-complete.o myoss.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

multipart-abort: multipart-abort.o myoss.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

multipart-upload-part: multipart-upload-part.o myoss.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $<


clean:
	rm -f $(TEMPFILES) $(PROG)


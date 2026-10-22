#ifndef _MBLUE_FIFO_H
#define _MBLUE_FIFO_H

#define is_power_of_2(x) ((x) != 0 && (((x) & ((x) - 1)) == 0))

struct mblue_fifo {
	unsigned char *buffer;	/* the buffer holding the data */
	unsigned int size;	/* the size of the allocated buffer */
	unsigned int in;	/* data is added at offset (in % size) */
	unsigned int out;	/* data is extracted from off. (out % size) */
	//void *lock;		/* lock to support mpmc */

	int (*init)(struct mblue_fifo *fifo, unsigned char *buffer, unsigned int size);
	unsigned int (*put)(struct mblue_fifo *fifo, unsigned char *buffer, unsigned int len);		
	unsigned int (*get)(struct mblue_fifo *fifo, unsigned char *buffer, unsigned int len);		
	unsigned int (*peek)(struct mblue_fifo *fifo, unsigned char *buffer, unsigned int len);		
	unsigned int (*len)(struct mblue_fifo *fifo);
	void (*reset)(struct mblue_fifo *fifo);
};

struct mblue_fifo *mblue_fifo_construct(void *buffer, unsigned int size);
struct mblue_fifo *mblue_fifo_alloc(unsigned int size);
void mblue_fifo_free(struct mblue_fifo *fifo);
#endif

#ifndef __GENERAL_LOAD_TARGET_H__
#define __GENERAL_LOAD_TARGET_H__

enum gl_device_e {
	GL_DEVICE_NONE,
	GL_DEVICE_RAM,
	GL_DEVICE_NET,
	GL_DEVICE_BLK,
	GL_DEVICE_MTD,
};

typedef struct gl_target_s gl_target_t;

#define gl_target_set_device(s, ifp) \
	s->set_device(s, ifp)
#define gl_target_get_type(s) \
	s->get_type(s)
#define gl_target_get_part(s) \
	s->get_part(s)
#define gl_target_get_desc(s) \
	s->get_desc(s)
#define gl_target_set_fmt(s, f) \
	s->set_fmt(s, f)
#define gl_target_get_fmt(s) \
	s->get_fmt(s)
#define gl_target_set_symbol(s, sym) \
	s->set_symbol(s, sym)
#define gl_target_get_symbol(s) \
	s->get_symbol(s)

struct gl_target_s {
	void* priv;
	// set_device(mmc0:1)
	int (*set_device)(gl_target_t* self, char* if_part);
	// GL_DEVICE_XXX
	int (*get_type)(gl_target_t* self);
	int (*get_part)(gl_target_t* self);
	void* (*get_desc)(gl_target_t* self);
	// set_fmt(fstype/proto)
	void (*set_fmt)(gl_target_t* self, int fmt);
	int (*get_fmt)(gl_target_t* self);
	// set_symbol
	void (*set_symbol)(gl_target_t* self, char* symbol);
	char* (*get_symbol)(gl_target_t* self);
};

gl_target_t* new_gl_target(void);
void destroy_gl_target(gl_target_t* target);

#endif

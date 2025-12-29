include mk/subdir_pre.mk

OBJS := $(addprefix $(d),if.o)

libubsim_objs += $(OBJS)

CLEAN := $(OBJS)
include mk/subdir_post.mk

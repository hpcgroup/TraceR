include ../Makefile.common

RM := rm -rf

USER_OBJS :=
LIBS := -lconv-bigsim-logs -lblue-standalone -lconv-util
SUBDIRS := . events entities

CPP_SRCS = TraceReader.C CWrapper.C
OBJS = TraceReader.o CWrapper.o
CPP_DEPS = TraceReader.d CWrapper.d

CPP_SRCS += entities/MsgEntry.C entities/PE.C entities/Task.C
OBJS += entities/MsgEntry.o entities/PE.o entities/Task.o
CPP_DEPS += entities/MsgEntry.d entities/PE.d entities/Task.d

events/%.o: events/%.C
	@echo 'Building file: $<'
ifeq ($(ARCH),bgq)
	$(SEQ_CXX) -I$(CHARMPATH)/include ${BASE_INCS} $(NETWORKOPTS) $(OPTS) $(WARNING) -c -fmessage-length=0 -M -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
else
	$(SEQ_CXX) -I$(CHARMPATH)/include ${BASE_INCS} $(NETWORKOPTS) $(OPTS) $(WARNING) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
endif
	@echo 'Finished building: $<'
	@echo ' '

entities/%.o: entities/%.C
	@echo 'Building file: $<'
ifeq ($(ARCH),bgq)
	$(SEQ_CXX) -I$(CHARMPATH)/include ${BASE_INCS} $(NETWORKOPTS) $(OPTS) $(WARNING) -c -fmessage-length=0 -M -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
else
	$(SEQ_CXX) -I$(CHARMPATH)/include ${BASE_INCS} $(NETWORKOPTS) $(OPTS) $(WARNING) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
endif
	@echo 'Finished building: $<'
	@echo ' '

%.o: %.C
	@echo 'Building file: $<'
ifeq ($(ARCH),bgq)
	$(SEQ_CXX) -I$(CHARMPATH)/include ${BASE_INCS} $(NETWORKOPTS) $(OPTS) $(WARNING) -c -fmessage-length=0 -M -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
else
	$(SEQ_CXX) -I$(CHARMPATH)/include ${BASE_INCS} $(NETWORKOPTS) $(OPTS) $(WARNING) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
endif
	@echo 'Finished building: $<'
	@echo ' '

fastSim: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	#$(LXX) -L$(CHARMPATH)/lib -o $@ $(NETWORKOPTS) $(OBJS) $(USER_OBJS) $(LIBS)
	@echo 'Finished building: $<'
	@echo ' '

clean:
	-$(RM) $(OBJS) $(C++_DEPS) $(CC_DEPS) $(CPP_DEPS) $(EXECUTABLES) $(CXX_DEPS) $(C_UPPER_DEPS) fastSim
	-@echo ' '

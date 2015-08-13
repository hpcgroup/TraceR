include ../Makefile.common

RM := rm -rf

USER_OBJS :=
LIBS := -lconv-bigsim-logs -lblue-standalone -lconv-util
SUBDIRS := . events entities

CPP_SRCS = TraceReader.cpp CWrapper.cpp
OBJS = TraceReader.o CWrapper.o
CPP_DEPS = TraceReader.d CWrapper.d

CPP_SRCS += entities/MsgEntry.cpp entities/PE.cpp entities/Task.cpp
OBJS += entities/MsgEntry.o entities/PE.o entities/Task.o
CPP_DEPS += entities/MsgEntry.d entities/PE.d entities/Task.d

ifneq ($(ARCH),bgq)
CXX = g++ 
endif

events/%.o: events/%.cpp
	@echo 'Building file: $<'
ifeq ($(ARCH),bgq)
	$(CXX) -I$(CHARMPATH)/include $(NETWORKOPTS) $(OPTS) $(WARNING) -c -fmessage-length=0 -M -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
else
	$(CXX) -I$(CHARMPATH)/include $(NETWORKOPTS) $(OPTS) $(WARNING) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
endif
	@echo 'Finished building: $<'
	@echo ' '

entities/%.o: entities/%.cpp
	@echo 'Building file: $<'
ifeq ($(ARCH),bgq)
	$(CXX) -I$(CHARMPATH)/include $(NETWORKOPTS) $(OPTS) $(WARNING) -c -fmessage-length=0 -M -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
else
	$(CXX) -I$(CHARMPATH)/include $(NETWORKOPTS) $(OPTS) $(WARNING) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
endif
	@echo 'Finished building: $<'
	@echo ' '

%.o: %.cpp
	@echo 'Building file: $<'
ifeq ($(ARCH),bgq)
	$(CXX) -I$(CHARMPATH)/include $(NETWORKOPTS) $(OPTS) $(WARNING) -c -fmessage-length=0 -M -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
else
	$(CXX) -I$(CHARMPATH)/include $(NETWORKOPTS) $(OPTS) $(WARNING) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o"$@" "$<"
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

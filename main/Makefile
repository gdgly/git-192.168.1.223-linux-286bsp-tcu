O_TARGET :=	main.o

#subdir-y +=	admin

subdir-y +=	data
subdir-y +=	lock
#subdir-y +=	meter

obj-y	:= cfminit.o $(join $(subdir-y), $(patsubst %,/%.o,$(notdir $(subdir-y))))
#obj-y	:= cfminit.o net-snmp/agent/snmp.a $(join $(subdir-y), $(patsubst %,/%.o,$(notdir $(subdir-y))))

include $(TOPDIR)/Rules.make

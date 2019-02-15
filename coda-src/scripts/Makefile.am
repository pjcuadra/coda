## Process this file with automake to produce Makefile.in

sbin_SCRIPTS =
dist_sbin_SCRIPTS =
dist_man_MANS =
if BUILD_CLIENT
sbin_SCRIPTS += coda-client-setup
dist_sbin_SCRIPTS += volmunge
dist_man_MANS += coda-client-setup.8 volmunge.8
endif
if BUILD_SERVER
sbin_SCRIPTS += bldvldb.sh startserver vice-setup vice-setup-rvm createvol \
		     purgevol
dist_sbin_SCRIPTS += codastart coda-server-logrotate \
		     partial-reinit.sh vice-killvolumes vice-setup-scm \
		     vice-setup-srvdir vice-setup-user
dist_man_MANS += bldvldb.sh.8 maxgroupid.5 startserver.8 vice-setup.8 \
		     volumelist.5 createvol.8 purgevol.8

if ENABLE_VOLUME_REPLICATION
sbin_SCRIPTS += createvol_rep purgevol_rep
dist_man_MANS += createvol_rep.8 purgevol_rep.8
endif

endif
EXTRA_DIST = findparents.sh volinfo.pl volsizes.pl
CLEANFILES = $(sbin_SCRIPTS)


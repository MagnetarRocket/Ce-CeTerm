############## CETERM setup
setenv PATH "/appl/ce_v262/bin:$PATH"
set ce = ce_v262
if ( ! ${?CEHOST} ) then
    setenv CEHOST ''
    if ( -e ~/.Cehost ) source ~/.Cehost
endif
if ( -e ~/.Cehost ) rm ~/.Cehost
if ( ! ${?CE_SH_PID} ) then
    setenv CE_SH_PID ""
    if ( $CEHOST != '' ) setenv CE_SH_PID $$
endif
setenv on_cehost "`echo $CEHOST | sed 's/\(.*\):.*/on \1/'`"
setenv p_pid "`echo $CE_SH_PID | sed 's/^[^ ]*/-p &/'`"
if ( ${?TERM} ) then
    unalias cd
    alias cd  'cd \!* ; set a = `pwd | sed "s/\/tmp_mnt//"` ; set prompt = "(`hostname`) ../$a:t \\!% " ; if (-e /appl/$ce/bin/xdmc && $TERM == vt100) $on_cehost /appl/$ce/bin/xdmc $p_pid "cd $a"; unset a '
    cd .
    stty intr  ^C
    stty erase ^H
    stty kill  ^U
endif
if ( $CEHOST != '' ) then
    alias rlogin 'set a = `pwd` ; echo "#\\!/bin/csh -f\nsetenv CEHOST $CEHOST\nsetenv CE_SH_PID $CE_SH_PID\ncd $a" > ~/.Cehost ; unset a ; /bin/rlogin'
else
    alias rlogin 'set a = `pwd` ; echo "#\\!/bin/csh -f\ncd $a" > ~/.Cehost ; unset a ; /bin/rlogin'
endif
##############
# 
# This allows ceterm to follow my directory changes when I am remote-logined,
# and to place me in the current working directory upon rlogin.  The ~/.Cehost
# file is used to store commands which set the CEHOST, CE_SH_PID and directory
# upon remote login.
# 
# --Tom v.T.

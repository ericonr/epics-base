#!/usr/bin/env perl
#*************************************************************************
# Copyright (c) 2008 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# Copyright (c) 2002 The Regents of the University of California, as
#     Operator of Los Alamos National Laboratory.
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# The makeTestfile.pl script generates a file $target.t which is needed
# because some versions of the Perl test harness can only run test scripts
# that are actually written in Perl.  The script we generate runs the
# real test program which must be in the same directory as the .t file.
# If the script is given an argument -tap it sets HARNESS_ACTIVE in the
# environment to make the epicsUnitTest code generate strict TAP output.

# Usage: makeTestfile.pl <target-arch> <host-arch> target.t executable
#     target-arch and host-arch are EPICS build target names (eg. linux-x86)
#     target.t is the name of the Perl script to generate
#     executable is the name of the file the script runs

# Test programs that need more than 500 seconds to run should have the
# EPICS_UNITTEST_TIMEOUT environment variable set in their Makefile:
#   longRunningTest.t: export EPICS_UNITTEST_TIMEOUT=3600
# That embeds the timeout into the .t file. The timeout variable can also
# be set at runtime, which will override any compiled-in setting but the
# 'make runtests' command can't give a different timeout for each test.

use 5.10.1;   # This script uses the defined-or operator //
use strict;

use File::Basename;
my $tool = basename($0);

my $timeout = $ENV{EPICS_UNITTEST_TIMEOUT} // 500; # 8 min 20 sec

my ($TA, $HA, $target, $exe) = @ARGV;
my ($exec, $error);

if ($TA =~ /^win32-x86/ && $HA !~ /^win/) {
    # Use WINE to run win32-x86 executables on non-windows hosts.
    # New Debian derivatives have wine32 and wine64, older ones have
    # wine and wine64. We prefer wine32 if present.
    my $wine32 = "/usr/bin/wine32";
    $wine32 = "/usr/bin/wine" if ! -x $wine32;
    $error = $exec = "$wine32 $exe";
}
elsif ($TA =~ /^windows-x64/ && $HA !~ /^win/) {
    # Use WINE to run windows-x64 executables on non-windows hosts.
    $error = $exec = "wine64 $exe";
}
elsif ($TA =~ /^RTEMS-pc[36]86-qemu$/) {
    # Run the pc386 and pc686 test harness w/ QEMU
    $exec = "qemu-system-i386 -m 64 -no-reboot "
        . "-serial stdio -display none "
        . "-net nic,model=e1000 -net nic,model=ne2k_pci "
        . "-net user,restrict=yes "
        . "-append --console=/dev/com1 "
        . "-kernel $exe";
    $error = "qemu-system-i386 ... -kernel $exe";
}
elsif ($TA =~ /^RTEMS-/) {
    # Explicitly fail for other RTEMS targets
    die "$tool: I don't know how to create scripts for testing $TA on $HA\n";
}
else {
    # Assume it's directly executable on other targets
    $error = $exec = "./$exe";
}

# Create the $target.t file
open(my $OUT, '>', $target)
    or die "$tool: Can't create $target: $!\n";

print $OUT <<__EOT__;
#!/usr/bin/env perl
# This file was generated by $tool

use strict;
use Cwd 'abs_path';
use File::Basename;
my \$tool = basename(\$0);

\$ENV{HARNESS_ACTIVE} = 1 if scalar \@ARGV && shift eq '-tap';
\$ENV{TOP} = abs_path(\$ENV{TOP}) if exists \$ENV{TOP};

# The timeout value below can be set in the Makefile that builds
# this test script. Add this line and adjust the value (in seconds):
#   $target: export EPICS_UNITTEST_TIMEOUT=$timeout
my \$timeout = \$ENV{EPICS_UNITTEST_TIMEOUT} // $timeout;
__EOT__

if ($^O eq 'MSWin32') {
    ######################################## Code for Windows run-hosts
    print $OUT <<__WIN32__;

use Win32::Process;
use Win32;

BEGIN {
  # Ensure that Windows interactive error handling is disabled.
  # This setting is inherited by the test process.
  # Set SEM_FAILCRITICALERRORS (1) Disable critical-error-handler dialog
  # Clear SEM_NOGPFAULTERRORBOX (2) Enabled WER to allow automatic post mortem debugging (AeDebug)
  # Clear SEM_NOALIGNMENTFAULTEXCEPT (4) Allow alignment fixups
  # Set SEM_NOOPENFILEERRORBOX (0x8000) Prevent dialog on some I/O errors
  # https://docs.microsoft.com/en-us/windows/win32/api/errhandlingapi/nf-errhandlingapi-seterrormode
  my \$sem = 'SetErrorMode';
  eval {
    require Win32::ErrorMode;
    Win32::ErrorMode->import(\$sem);
  };
  eval {
    require Win32API::File;
    Win32API::File->import(\$sem);
  } if \$@;
  SetErrorMode(0x8001) unless \$@;
}

my \$proc;
if (! Win32::Process::Create(\$proc, abs_path('$exec'),
    '$exec', 1, NORMAL_PRIORITY_CLASS, '.')) {
    my \$err = Win32::FormatMessage(Win32::GetLastError());
    die "\$tool: Can't create Process for '$error': \$err\\n";
}
if (! \$proc->Wait(1000 * \$timeout)) {
    \$proc->Kill(1);
    print "\\n#### Test stopped by \$tool after \$timeout seconds\\n";
    die "\$tool: Timed out '$error' after \$timeout seconds\\n";
}
my \$status;
\$proc->GetExitCode(\$status);
exit \$status;

__WIN32__
}
else {
    ######################################## Code for Unix run-hosts
    print $OUT <<__UNIX__;

use POSIX qw(WIFEXITED WIFSIGNALED WEXITSTATUS);

my \$pid = fork();
die "\$tool: Can't fork for '$error': \$!\\n"
    unless defined \$pid;

if (\$pid) {
    # Parent process
    \$SIG{ALRM} = sub {
        # Time's up, kill the child
        kill 9, \$pid;
        print "\\n#### Test stopped by \$tool after \$timeout seconds\\n";
        die "\$tool: Timed out '$error' after \$timeout seconds\\n";
    };

    alarm \$timeout;
    while (1) {
        waitpid \$pid, 0;
        if (WIFEXITED(\$?)) {
            # normal exit
            alarm 0;
            exit WEXITSTATUS(\$?);
        } elsif (WIFSIGNALED(\$?)) {
            # terminated by signal
            alarm 0;
            die "\$tool: Test was terminated by signal '\$?'\\n";
        }
        # non-terminal change of status, continue waiting
    }
}
else {
    # Child process
    exec '$exec'
        or die "\$tool: Can't run '$error': \$!\\n";
}
__UNIX__
}

close $OUT
    or die "$tool: Can't close '$target': $!\n";

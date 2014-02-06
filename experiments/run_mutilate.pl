#!/usr/bin/perl

#
# run_mutilate.pl - Experiment framework for running mutilate and memcached
#
# Uses SSH to enlist the help of several machines, including a memcached
# server, a mutilate master, and one or more mutilate agents. Runs mutilate
# tests in agent mode and reports test results to stdout. All other messages
# (diagnostic, status, etc.) are printed to stderr, so redirecting stdout will
# capture only the mutilate test results.
#
# SSH connections are made in the name of the current user. For best results
# ensure all machines used in the tests support public key authentication with
# the current user from the machine on which this script is executed. This will
# avoid password prompts, which may cause the script to report timeouts.
#
# Currently parameters can only be changed by editing the configuration
# variables at the top of the script. Defaults have been supplied based on what
# has experimentally been shown to produce good results.
#

use threads;
use threads::shared;

$version                = "0.1 (05-Feb-2014)";

# Configuration variables
$timeout                = 7;        # Timeout for SSH tests, in seconds
$maintimeout            = 60;       # Timeout for mutilate itself, in seconds
$memcached_numconn      = 32768;    # Number of connections supported on the memcached server (-c)
$mutilate_updateratio   = "0.0";    # Ratio of set:get commands to issue during the test
$mutilate_testlength    = 20;       # Number of seconds to run the mutilate test (-t)

# Shared variables for inter-thread communication
my $test_status         :shared = 1;
my $memcached_status    :shared = 1;
my @agent_status        :shared;

# Helper subroutines for printing information about this script
sub printdashline() {
    print {STDERR} "---------------------------------------------------------------------------\n"
}

sub printname() {
    print {STDERR} "\nMutilate Test Runner v$version\n";
}

sub printusage() {
    print "usage: $0 memcached_host master agent1 [agent2] ... [agentN]\n\n";
}

sub printtesterror($) {
    print {STDERR} "[ERROR]\n\n";
    printdashline();
    print {STDERR} "SSH connectivity test failed. This likely means one of the servers is not\n";
    print {STDERR} "configured correctly for running tests. Error text is below.\n\n";
    print {STDERR} "$_[0]";
    print {STDERR} "\n";
}

# Thread subroutine for connecting, via supplied SSH command, to the memcached server
sub thread_connect_memcached($) {
    my $command = $_[0];
    $temp_status = `$command`;
    $retcode = $? >> 8;
    $memcached_status = $temp_status;
    return $retcode;
}

# Thread subroutine for connecting, via supplied SSH command, to a mutilate agent
sub thread_connect_agent($$) {
    my $command = $_[0];
    my $agentid = $_[1];
    $temp_status = `$command`;
    $retcode = $? >> 8;
    $agent_status[$agentid] = $temp_status;
    return $retcode;
}

printname();

# Verify the command line parameters are correctly present
if ($#ARGV < 2) {
    printusage();
    exit(-1);
}

print {STDERR} "\n";
printdashline();

# Extract the required information about which servers to use
my $memcached_server    = $ARGV[0];
my $mutilate_master		= $ARGV[1];
my @mutilate_agents;

for ($i = 2; $i <= $#ARGV; $i++) {
    push(@mutilate_agents, $ARGV[$i]);
    push(@agent_status, 1);
}

print {STDERR} "Using memcached server:     $memcached_server\n";
print {STDERR} "Using mutilate master:      $mutilate_master\n";
print {STDERR} "Using mutilate agents:      @mutilate_agents\n";
printdashline();

# Test SSH connectivity to the servers that should be used
local $SIG{ALRM} = sub {
    print {STDERR} "[TIMEOUT]\n\n";
    printdashline();
    print {STDERR} "SSH connectivity test failed due to a timeout. Verify that public key\n";
    print {STDERR} "authentication works for all the servers needed for the test and try again.\n";
    print {STDERR} "\n";
    exit(1);
};

print {STDERR} "\nTesting SSH connectivity...\n";

# First the memcached server
print {STDERR} "        $memcached_server";
for ($j = 0; $j < (57 - length($memcached_server)); $j++) {
    print {STDERR} " ";
}
alarm($timeout);
$testresult = `ssh $memcached_server PATH=\"$ENV{PATH}\" memcached -h 2>&1`;
$teststatus = ($? >> 8);
alarm(0);
if ($teststatus != 0) {
    printtesterror($testresult);
    exit(1);
}
print {STDERR} "[OK]\n";

# Next the mutilate master
print {STDERR} "        $mutilate_master";
for ($j = 0; $j < (57 - length($mutilate_master)); $j++) {
    print {STDERR} " ";
}
alarm($timeout);
$testresult = `ssh $mutilate_master PATH=\"$ENV{PATH}\" mutilate --version 2>&1`;
$teststatus = ($? >> 8);
alarm(0);
if ($teststatus != 0) {
    printtesterror($testresult);
    exit(1);
}
print {STDERR} "[OK]\n";

# Finally the mutilate agents
for ($i = 0; $i <= $#mutilate_agents; $i++) {
    print {STDERR} "        $mutilate_agents[$i]";
    for ($j = 0; $j < (57 - length($mutilate_agents[$i])); $j++) {
        print {STDERR} " ";
    }
    alarm($timeout);
    $testresult = `ssh $mutilate_agents[$i] PATH=\"$ENV{PATH}\" mutilate --version 2>&1`;
    $teststatus = ($? >> 8);
    alarm(0);
    if ($teststatus != 0) {
        printtesterror($testresult);
        exit(1);
    }
    print {STDERR} "[OK]\n";
}

# Build the command strings for executing the tests
my $memcached_cmd = "ssh $memcached_server PATH=\"$ENV{PATH}\" memcached -t \\\`nproc\\\` -c $memcached_numconn 2>&1";
my $master_cmd    = "ssh $mutilate_master PATH=\"$ENV{PATH}\" mutilate -s ${memcached_server} -B -t $mutilate_testlength -u $mutilate_updateratio -T \\\`nproc\\\` -D \\\`nproc\\\` -C \\\`nproc\\\` -c \\\`nproc\\\` -Q 1000 2>&1";
my @agent_cmd;
for ($i = 0; $i <= $#mutilate_agents; $i++) {
    $master_cmd = "$master_cmd -a $mutilate_agents[$i]";
    push(@agent_cmd, "ssh $mutilate_agents[$i] PATH=\"$ENV{PATH}\" mutilate -T \\\`nproc\\\` -A 2>&1");
}

# Start up memcached
print {STDERR} "\nStarting memcached...                                            ";
my $memcached_thread = threads->create('thread_connect_memcached', "$memcached_cmd");
threads->yield();
print {STDERR} "[OK]\n";

# Start up the agents
print {STDERR} "\nStarting mutilate agents...                                      ";
my @agent_threads;
for ($i = 0; $i <= $#mutilate_agents; $i++) {
    push(@agent_threads, threads->create('thread_connect_agent', "$agent_cmd[$i]", $i));
    threads->yield();
}
print {STDERR} "[OK]\n";

# Start the testing
local $SIG{ALRM} = sub {
    print {STDERR} "[TIMEOUT]\n";
    
    # Clean up even though the tests ended prematurely
    print {STDERR} "\nTesting timed out, and processes have been cleanly killed.\nError details (if any) follow.\n";
    printdashline();
    
    # Check which threads left the mutilate master hanging and print the errors
    if ($memcached_status != 1) {
        print {STDERR} "Memcached aborted due to an error:\n$memcached_status\n";
        $memcached_thread->join();
    } else {
        $memcached_thread->detach();
    }
    
    for ($i = 0; $i <= $#mutilate_agents; $i++) {
        if ($agent_status[$i] != 1) {
            print {STDERR} "Agent on $mutilate_agents[$i] aborted due to an error:\n$agent_status[$i]\n";
            $agent_threads[$i]->join();
        } else {
            $agent_threads[$i]->detach();
        }
    }
    
    # Clean up stray processes
    `ssh $memcached_server killall memcached 2>&1`;
    `ssh $mutilate_master killall mutilate 2>&1`;
    for ($i = 0; $i <= $#mutilate_agents; $i++) {
        `ssh $mutilate_agents[$i] killall mutilate 2>&1`;
    }
    
    exit(1);
};

print {STDERR} "\nRunning mutilate test...                                         ";
alarm($maintimeout);
$results = `$master_cmd`;
alarm(0);
print {STDERR} "[OK]\n";

# Clean up: detach all threads and kill all memcached/mutilate processes
print {STDERR} "\nCleaning up...                                                   ";
$memcached_thread->detach();
for ($i = 0; $i <= $#mutilate_agents; $i++) {
    $agent_threads[$i]->detach();
}
`ssh $memcached_server killall memcached 2>&1`;
`ssh $mutilate_master killall mutilate 2>&1`;
for ($i = 0; $i <= $#mutilate_agents; $i++) {
    `ssh $mutilate_agents[$i] killall mutilate 2>&1`;
}
print {STDERR} "[OK]\n\nTesting completed.\n";
printdashline();
print "$results";
print {STDERR} "\n";
#!/usr/bin/perl
use strict;

my $OUTPUT_FILE = "parallel_maxflow.output";
my $alg = int($ARGV[0])-1;

sub run_command # @params: cmd
{
    system("echo '---------- beginning command: " . $_[0] . "' >>" . $OUTPUT_FILE.".". $alg);
    system($_[0]);
    print $_[0],"\n" ;
    system("echo '---------- ending    command: " . $_[0] . "' >>" . $OUTPUT_FILE.".". $alg);
    system("echo >>" . $OUTPUT_FILE.".". $alg);
    system("echo >>" . $OUTPUT_FILE.".". $alg);
}
sub run_goldberg_algorithms # @params: inputfile
{
    foreach ("./f_prf.exe", "./h_prf.exe", "./hi_pr", "./hi_pro", "./hi_prw")
    {
        my $cmd = "cat " . $_[0] . " | /usr/bin/time -v " . $_ . " >>" . $OUTPUT_FILE.".". $alg . " 2>>" . $OUTPUT_FILE.".". $alg;
        run_command($cmd);
    }
}
sub run_our_algorithms # @params: inputfile
{
    foreach (1..12)
    {
        my $cmd = "/usr/bin/time -v ./fprf_cpp_tbb " . $_[0] . " " . $_ . " >>" . $OUTPUT_FILE.".". $alg . " 2>>" . $OUTPUT_FILE.".". $alg;
        run_command($cmd);
    }
}

my @inputs = (
    {
        name => "genrmf-wide",
        cmd => "./genrmf -out FILE -seed SEED -a 400 -b 16 -c1 1 -c2 10000",
        file => "genrmf_wide_400_16_1_10000"
    },
    {
        name => "genrmf-long",
        cmd => "./genrmf -out FILE -seed SEED -a 32 -b 1024 -c1 1 -c2 10000",
        file => "genrmf_long_32_1024_1_10000"
    },
    {
        name => "washington-rlg-wide",
        cmd => "./washington 2 32768 64 10000 SEED > FILE",
        file => "wash_wide_2_32768_64_10000"
    },
    {
        name => "washington-rlg-long",
        cmd => "./washington 2 64 65536 10000 SEED > FILE",
        file => "wash_long_2_64_65536_10000"
    },
    {
        name => "washington-line-moderate",
        cmd => "./washington 6 65536 4 128 SEED > FILE",
        file => "wash_moderate_6_65536_4_128"
    },
    {
        name => "ac-dense",
        cmd => "./ac 8192 10000 SEED > FILE",
        file => "ac_dense_8192_10000"
    },


    {
        name => "BL06-gargoyle-lrg.max",
        cmd => "wget --continue 'http://vision.csd.uwo.ca/data/maxflow/multiview/BL06-gargoyle-lrg.tbz2' && tar -jxvf BL06-gargoyle-lrg.tbz2",
        file => "BL06-gargoyle-lrg.max"
    },
    {
        name => "BL06-gargoyle-med.max",
        cmd => "wget --continue 'http://vision.csd.uwo.ca/data/maxflow/multiview/BL06-gargoyle-med.tbz2' && tar -jxvf BL06-gargoyle-med.tbz2",
        file => "BL06-gargoyle-med.max"
    },
    {
        name => "LB07-bunny-med.max",
        cmd => "wget --continue 'http://vision.csd.uwo.ca/data/maxflow/surfacefit/LB07-bunny-med.tbz2' && tar -jxvf LB07-bunny-med.tbz2",
        file => "LB07-bunny-med.max"
    },
    {
        name => "babyface.n6c100.max",
        cmd => "wget --continue 'http://vision.csd.uwo.ca/data/maxflow/segmentation/babyface.n6c100.tbz2' && tar -jxvf babyface.n6c100.tbz2",
        file => "babyface.n6c100.max"
    },
    {
        name => "babyface.n6c10.max",
        cmd => "wget --continue 'http://vision.csd.uwo.ca/data/maxflow/segmentation/babyface.n6c10.tbz2' && tar -jxvf babyface.n6c10.tbz2",
        file => "babyface.n6c10.max"
    },
    {
        name => "BL06-camel-lrg.max",
        cmd => "wget --continue 'http://vision.csd.uwo.ca/data/maxflow/multiview/BL06-camel-lrg.tbz2' && tar -jxvf BL06-camel-lrg.tbz2",
        file => "BL06-camel-lrg.max"
    },
    {
        name => "BL06-camel-med.max",
        cmd => "wget --continue 'http://vision.csd.uwo.ca/data/maxflow/multiview/BL06-camel-med.tbz2' && tar -jxvf BL06-camel-med.tbz2",
        file => "BL06-camel-med.max"
    },

    # 13
    {
        name => "genrmf-wide",
        cmd => "./genrmf -out FILE -seed SEED -a 194 -b 14 -c1 1 -c2 10000",
        file => "genrmf_wide_194_14_1_10000"
    },
    {
        name => "genrmf-wide",
        cmd => "./genrmf -out FILE -seed SEED -a 147 -b 12 -c1 1 -c2 10000",
        file => "genrmf_wide_147_12_1_10000"
    },

    {
        name => "genrmf-long",
        cmd => "./genrmf -out FILE -seed SEED -a 27 -b 724 -c1 1 -c2 10000",
        file => "genrmf_long_27_724_1_10000"
    },
    {
        name => "genrmf-long",
        cmd => "./genrmf -out FILE -seed SEED -a 23 -b 512 -c1 1 -c2 10000",
        file => "genrmf_long_23_512_1_10000"
    },

    {
        name => "washington-rlg-wide",
        cmd => "./washington 2 16384 64 10000 SEED > FILE",
        file => "wash_wide_2_16384_64_10000"
    },
    {
        name => "washington-rlg-wide",
        cmd => "./washington 2 8192 64 10000 SEED > FILE",
        file => "wash_wide_2_8192_64_10000"
    },

    {
        name => "washington-rlg-long",
        cmd => "./washington 2 64 32768 10000 SEED > FILE",
        file => "wash_long_2_64_32768_10000"
    },
    {
        name => "washington-rlg-long",
        cmd => "./washington 2 64 16384 10000 SEED > FILE",
        file => "wash_long_2_64_16384_10000"
    },

    {
        name => "washington-line-moderate",
        cmd => "./washington 6 32768 4 90 SEED > FILE",
        file => "wash_moderate_6_32768_4_90"
    },
    {
        name => "washington-line-moderate",
        cmd => "./washington 6 16384 4 64 SEED > FILE",
        file => "wash_moderate_6_16384_4_64"
    },

    {
        name => "ac-dense",
        cmd => "./ac 4096 10000 SEED > FILE",
        file => "ac_dense_4096_10000"
    },
    {
        name => "ac-dense",
        cmd => "./ac 2048 10000 SEED > FILE",
        file => "ac_dense_2048_10000"
    }
);

sub run_goldberg_test_suite # @param index(into @inputs)
{
    my $input = $inputs[$_[0]];
    foreach(0..9) # seeds
    {
        my $file = $input->{'file'} . "_" . $_;
        my $cmd = $input->{'cmd'};
        $cmd =~ s/SEED/$_/;
        $cmd =~ s/FILE/$file/;

        system($cmd);

        foreach(0..2) # we run 3 times on each instance
        {
            run_goldberg_algorithms($file);
            run_our_algorithms($file);
        }
        system("rm " . $file);
    }
}

sub run_vision_test_suite # @param index(into @inputs)
{
    my $input = $inputs[$_[0]];
    system($input->{'cmd'});
    foreach(0..9) # we run 10 times on each instance
    {
        run_goldberg_algorithms($input->{'file'});
        run_our_algorithms($input->{'file'});
    }
    system("rm " . $input->{'file'});
}

sub print_usage
{
    my $str = "usage:\n";
    foreach (0..$#inputs)
    {
        $str = $str . "\t./run.pl $_\n\t\tfor running on $inputs[$_]{'name'} ($inputs[$_]{'file'})\n";
    }
    print $str;
    system("echo '$str' >>" . $OUTPUT_FILE.".". $alg);
}

system("echo BEGINNING TEST >> $OUTPUT_FILE.$alg");
if (scalar(@ARGV) == 0)
{
    print_usage();
}
else
{
    #$alg = int($ARGV[0])-1;
    if ($alg >= 0 and $alg <= $#inputs)
    {
        system("echo '\tTEST SUITE: $alg' >> $OUTPUT_FILE.$alg");
        if ($alg > 12)
        {
            run_goldberg_test_suite($alg);
        }
        elsif ($alg > 5)
        {
            run_vision_test_suite($alg);
        }
        else
        {
            run_goldberg_test_suite($alg);
        }
    }
    else
    {
        print_usage();
    }
}
system("echo ENDING TEST >> $OUTPUT_FILE.$alg");


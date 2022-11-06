use strict;
use warnings;
use utf8;
use Getopt::Long 'GetOptions';
use POSIX 'strftime';

binmode STDOUT, ':encoding(utf8)';

my $version = "5.0";
my $svn = "svn";
my $git = "git";
my $out_header = "svnversion.h";
my $out_bat = "sourcetree_info.bat";
my $out_cmake = "";
my $source_root = "..";
my $date = strftime "%Y%m%d", localtime;
my $time = strftime "%H%M%S", localtime;
my $verbose = 0;
my $script_name = $0;
my $header = "This file was generated by buildtools/svnrev/svnrev.pl";
my %svninfo = (
	name => '',
	release => 0,
	Revision => ''
	);

sub read_toolinfo {
	my $info = "toolinfo.txt";
	if (!-f $info) {
		return;
	}
	open(my $FD, "<:utf8:crlf", $info);
	while (my $l = <$FD>) {
		chomp $l;
		$l =~ s/^\x{FEFF}//;	# remove BOM
		if ($l =~ /^#/) {
			next;
		}
		if ($l =~ /^svn=\s*(.*)$/) {
			$svn = $1;
		}
		if ($l =~ /^git=\s*(.*)$/) {
			$git = $1;
		}
	}
	close($FD);
}

sub search_svn {
	my @test_list = (
		"C:/Program Files (x86)/Subversion/bin/svn.exe",
		"C:/Program Files/Subversion/bin/svn.exe",
		"C:/Program Files/TortoiseSVN/bin/svn.exe",
		"C:/cygwin64/bin/svn.exe",
		"C:/cygwin/bin/svn.exe",
		"/usr/bin/svn.exe"	# msys, cygwin
		);
	for (my $i = 0; $i < @test_list; $i++) {
		my $test = $test_list[$i];
		if (-e $test) {
			$svn = $test;
			return;
		}
	}
}

sub search_git {
	my @test_list = (
		"C:/Program Files/Git/bin/git.exe",
		"C:/cygwin64/bin/git.exe",
		"C:/cygwin/bin/git.exe",
		"/usr/bin/git.exe"	# msys, cygwin
		);
	for (my $i = 0; $i < @test_list; $i++) {
		my $test = $test_list[$i];
		if (-e $test) {
			$git = $test;
			return;
		}
	}
}

sub dump_info()
{
	my %info = @_;

	print "SVNREVISION $info{'Revision'}\n";
	print "RELEASE $info{'release'}\n";
	print "BRANCH_NAME $info{'name'}\n";
}

sub write_info_header
{
	my ($out_header, %svninfo) = @_;
	my $revision = $svninfo{'Revision'};

	open(my $FD, ">$out_header") || die "error $out_header";
	print $FD "/* $header */\n";
	print $FD "/* #define TT_VERSION_STR \"$version\" check teraterm/common/tt-version.h */\n";
	if ($revision ne '') {
		print $FD "#define SVNVERSION $revision\n";
	} else {
		print $FD "#undef SVNVERSION\n";
	}
	if ($svninfo{'release'}) {
		print $FD "#define TERATERM_RELEASE 1\n";
	} else {
		print $FD "#undef TERATERM_RELEASE\n";
	}
	print $FD "#define BRANCH_NAME \"$svninfo{'name'}\"\n";
	close($FD);
}

sub write_info_bat
{
	my ($out_header, %svninfo) = @_;
	my $revision = $svninfo{'Revision'};

	open(my $FD, ">$out_bat") || die "error $out_bat";
	print $FD "\@rem $header\n";
	print $FD "set VERSION=$version\n";
	if ($revision ne '') {
		print $FD "set SVNVERSION=$revision\n";
	} else {
		print $FD "set SVNVERSION=unknown\n";
	}
	print $FD "set RELEASE=$svninfo{'release'}\n";
	print $FD "set DATE=$date\n";
	print $FD "set TIME=$time\n";
	close($FD);
}

sub write_info_cmake
{
	my ($out_header, %svninfo) = @_;
	my $revision = $svninfo{'Revision'};

	open(my $FD, ">$out_cmake") || die "error $out_cmake";
	print $FD "# $header\n";
	print $FD "set(VERSION \"$version\")\n";
	if ($revision ne '') {
		print $FD "set(SVNVERSION \"$revision\")\n";
	} else {
		print $FD "#set(SVNVERSION \"0000\")\n";
	}
	print $FD "set(RELEASE $svninfo{'release'})\n";
	print $FD "set(DATE \"$date\")\n";
	print $FD "set(TIME \"$time\")\n";
	close($FD);
}

sub read_revision_from_header()
{
	my ($out_header) = @_;

	# ヘッダーファイルがない場合
	if (! -f $out_header) {
		return '';
	}

	open FH, '<', $out_header || die "error open $out_header";
	while (<FH>) {
		if (/#define SVNVERSION (\d+)/) {
			close FH;
			return $1;
		}
	}
	close FH;

	# パターンマッチしない場合
	return '';
}

&search_svn();
&search_git();
&read_toolinfo();

GetOptions(
	'root=s' => \$source_root,
	'svn=s' => \$svn,
	'git=s' => \$git,
	'header=s' => \$out_header,
	'bat=s' => \$out_bat,
	'cmake=s' => \$out_cmake,
	'verbose' => \$verbose
);

$git =~ s/"//g;
$git =~ s/\\/\//g;
$svn =~ s/"//g;
$svn =~ s/\\/\//g;

if ($verbose != 0) {
	print "root=$source_root\n";
	print "svn=\"$svn\"\n";
	print "git=\"$git\"\n";
	print "header=\"$out_header\"\n";
	print "bat=\"$out_bat\"\n";
	print "cmake=\"$out_cmake\"\n";
}

if (-d "$source_root/.svn" && $svn ne "") {
	# svn infoを実行、出力をすべて取り込む
	if (!open(my $FD, "-|", "\"$svn\" info --xml $source_root 2>&1")) {
		# svn が実行できない
		print "$script_name: '$svn' can not execute\n";
	}
	else {
		# 出力をすべて取り込む
		my $text = do { local $/; <$FD> };
		close($FD);

		# xmlパーサがインストールされていない環境もあるので
		# パターンマッチで実装
		if ($text =~ /<commit([^>]+)>/) {
			my $commit = $1;
			if ($commit =~ /revision=\"(\d+)\"/) {
				$svninfo{'Revision'} = $1;
			}
		}
		if ($text =~ /<relative-url>(.+)<\/relative-url>/) {
			my $url = $1;
			my $name = $url;
			$name =~ s/^\^\/(.*)$/$1/; # "\/" を削除する
			$svninfo{'name'} = $name;
			my $release = 0;
			if ($url =~ /tags\/(teraterm-(\d+)_(\d+))/) {
				# パス名からリリースと判定
				my $tag = $1;
				my $version = "$2.$3";
				$release = 1;
			}
			$svninfo{'release'} = $release;
		}
	}
}
elsif(-d "$source_root/.git" && $git ne "") {
	my $branch = `\"$git\" rev-parse --abbrev-ref HEAD`;
	if ($branch eq '') {
		# git が実行できない
		print "$script_name: \"$git\" can not execute\n";
	}
	else {
		$branch =~ s/[\r\n]$//g;
		$svninfo{'name'} = $branch;

		if (-d "$source_root/.git/svn") {
			# use git svn log
			my $revision = `\"$git\" svn log --oneline -1`;
			$revision =~ s/^r(\d+).*$/$1/;
			$svninfo{'Revision'} = $1;
		}
		else {
			$svninfo{'Revision'} = '';
		}

		my $release = 0;
		if ($branch =~ /tags\/(teraterm-(\d+)_(\d+))/) {
			# リリースと判定
			my $tag = $1;
			my $version = "$2.$3";
			$release = 1;
		}
		$svninfo{'release'} = $release;
	}
}
else {
	# do not use VCS
}

if ($verbose != 0) {
	&dump_info(%svninfo);
}

# read revision from out header file
my $header_revision = &read_revision_from_header($out_header);
if ($verbose != 0) {
	print "Revision of header is '$header_revision'\n";
}

# output for source(C,C++) header
if (! -f $out_header ||
    $header_revision ne $svninfo{'Revision'}) {
	if ($verbose != 0) {
		print "write '$out_header'\n";
	}
	write_info_header($out_header, %svninfo);
}

# output for bat file
if ($verbose != 0) {
	print "write '$out_bat'\n";
}
write_info_bat($out_bat, %svninfo);

# output for cmake
if ($out_cmake ne "") {
	if ($verbose != 0) {
		print "write '$out_cmake'\n";
	}
	write_info_cmake($out_cmake, %svninfo);
}

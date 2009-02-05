require 'test/unit'
require File.dirname(File.expand_path(__FILE__)) + '/runTests.rb'

def mock(*args)
  CheekyMock.new(*args)
end

class CheekyMock
  
  attr_reader :stubs, :received
  
  def initialize(of)
    @of = of
    @received = []
    @stubs = {}
  end
  
  def method_missing(name, *params)
    name = name.to_sym
    @received << {:method => name, :with => params}
    raise 'Unexpected call to ' + name.to_s + ' in mock '  + @of unless @stubs.has_key?(name)
    @stubs[name]
  end
  
  def []=(name, value)
    @stubs[name.to_sym] = value
  end
  
  def [](name) @stubs[name.to_sym] end
    
end

class TestSections < Test::Unit::TestCase
  
  def test_it_can_pull_sections_from_a_string
    assert_equal(
      [{:title => "Foo", :content => "foo content"},
       {:title => "Bar", :content => "bar\n\n== nothing\n\ncontent"},
       {:title => "Zim", :content => "zim content"}], 
      Sections.new("=== Foo\n\nfoo content\n\n=== Bar\n\nbar\n\n== nothing\n\ncontent\n\n=== Zim\n\nzim content").sections
    )
    assert_equal(
      [{:title => 'Foo', :content => 'foo'},
       {:title => 'Pie', :content => 'pie'}], 
      Sections.new("\n\n=== Foo\nfoo\n\n=== Pie\n\npie\n").sections
    )
  end
  
  def test_it_provides_program_and_output_and_err
    sections = Sections.new("=== Foo\n\nfoo\n=== program\n\nprogram content\n\n=== bar\n\nbar\n=== exPected  stdout\n\noutput\n=== zim\n\nzim\n")
    assert_equal(sections.program, "program content")
    assert_equal(sections.expected_stdout, "output")
    sections = Sections.new("===  Program\n\n1234\n\n=== Expected Stdout\n\n5678\n=== Expected Stderr\n\nerr")
    assert_equal(sections.program, "1234")
    assert_equal(sections.expected_stdout, "5678")
    assert_equal(sections.expected_stderr, 'err')
  end
  
end
class TestRunner < Test::Unit::TestCase
  
  def runner_test(equal_out, equal_err, should_be)
    @gs = mock('Gospel')
    @gs[:run] = [out = rand.to_s, err = rand.to_s]
    @sections = mock('Sections')
    @sections[:program] = @program = rand
    @sections[:expected_stdout] = equal_out ? out : rand.to_s
    @sections[:expected_stderr] = equal_err ? err : rand.to_s
    assert_equal(should_be, Runner.new(@gs, @sections).run.result)
    assert_equal({:method => :program, :with => []}, @sections.received[0])
    assert(@sections.received[1][:method] == :expected_stdout || @sections.received[1][:method] == :expected_stderr)
    assert_equal({:method => :run, :with => [@program]}, @gs.received[0])
  end
  
  def test_execute_program_compare
    runner_test(true,  true,  true )  # equal
    runner_test(false, false, false)  # different
    runner_test(true,  false, false)  # partly equal
    runner_test(false, true,  false)  # partly equal
  end
  
end

require 'shell'

class TestShellExec < Test::Unit::TestCase
  
  def setup
    File.open(@filename = File.expand_path('.') + '/stdErrAndStdOut', 'w') do |f|
      f << "#!/usr/bin/env sh\necho foo\necho bar 1>&2"
      f.chmod(0744)
    end
  end
  
  def teardown
    File.unlink(@filename)
  end
  
  def test_integration
    out, err = ShellExec.new.exec(@filename)
    assert_equal('foo', out.strip)
    assert_equal('bar', err.strip)
  end
  
end

class TestGospel < Test::Unit::TestCase
  
  def test_it_should_save_programs_to_temp_file_and_run_temp_with_gopel_using_shellexec
    gs = Gospel.new
    gs.file_factory = f_fact = mock('Tempfile')
    gs.shell = mock('ShellExec')
    gs.gospel_path = rand.to_s
    gs.shell[:exec] = shell_return = rand
    f_fact[:new] = f = mock('file')
    f[:path] = fpath = rand.to_s
    f[:close] = f[:<<] = nil
    assert_equal(shell_return, gs.run(program = rand.to_s))
    assert_equal(:new, f_fact.received[0][:method])
    assert_equal({:method => :<<, :with => [program]}, f.received[0])
    assert_equal(:close, f.received[1][:method])
    assert_equal(:path, f.received[2][:method])
    assert_equal({:method => :exec, :with => [gs.gospel_path + ' ' + fpath]}, gs.shell.received[0])
  end
  
end
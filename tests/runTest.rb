

class Sections

  attr_reader :str, :sections

  def initialize(str)
    @str = str.strip
    @sections = parse
  end
  
  def parse
    split.map do |split|
      {:title => split.first.strip, :content => split[1..-1].join.strip}
    end.compact
  end
  
  def program
    @sections.find { |x| x[:title] =~ /^program$/i }[:content]
  end
  
  def expected_stdout
    @sections.find { |x| x[:title] =~ /^expected\s+stdout$/i }[:content]
  end
  
  def expected_stderr
    @sections.find { |x| x[:title] =~ /^expected\s+stderr$/i }[:content]
  end
  
  protected
  
  def split
    @str.split(/^=== +(?=\w)/).map { |x| x.to_a unless x.empty? }.compact
  end
  
end

# Doing two things: running and reporting
class Runner

  def initialize(interpreter, sections)
    @sections = sections
    @interpreter = interpreter
  end

  def run
    @out, @err = @interpreter.run(@sections.program)
    self
  end
  
  def result
    stdout_matches_expectations && stderr_matches_expectations
  end
  
  def stdout_matches_expectations
    @out.strip == @sections.expected_stdout.strip
  end
  
  def stderr_matches_expectations
    @err.strip == @sections.expected_stderr.strip
  end
  
  def report
    out = ''
    unless stdout_matches_expectations
      out = '=== ' + @sections.sections.first[:title]
      out << "\n\n=== Expected Stdout\n\n#{@sections.expected_stdout}\n\n=== Actual Stdout\n\n#@out"
    end
    unless stderr_matches_expectations
      out = '=== ' + @sections.sections.first[:title] if out.empty?
      out << "\n\n=== Expected Stderr\n\n#{@sections.expected_stderr}\n\n=== Actual Stderr\n\n#@err"
    end
    out + "\n"
  end

end

class Gospel

  require 'tempfile'

  attr_accessor :shell, :gospel_path, :file_factory

  def initialize
    @gospel_path = './gospel'
    @shell = ShellExec.new
    @file_factory = Tempfile
  end
  
  def run(program)
    program += "\nexit\n" # currently required to prevent entering REPL
    f = @file_factory.new('gs_test_program')
    f << program 
    f.close
    @shell.exec(@gospel_path + ' ' + f.path)
  end

end

class ShellExec
  
  require 'open3'
  
  # returns an array of two strings, the first representing stdout, the
  # second representing stderr
  def exec(*command)
    Open3.popen3(*command)[1..-1].map { |io| io.read }
  end
  
end

def run(test_file)
  sections = Sections.new(File.read(test_file))
  if (r = Runner.new(Gospel.new, sections).run).result == false
    print r.report
  end
end

if $0 == __FILE__
  test_files.each do |test_file|
    run test_file
  end
end
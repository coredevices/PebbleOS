/*
  Memfault compact log format strings section.
  Read more at https://mflt.io/compact-logs

  This section is not loaded to memory - it only exists in the ELF for
  post-processing by the Memfault cloud to decode compact log entries.
*/

SECTIONS
{
  log_fmt 0xF0000000 (INFO) :
  {
      __start_log_fmt = ABSOLUTE(.);
      KEEP(*(*.log_fmt_hdr))
      KEEP(*(log_fmt))
  }
}

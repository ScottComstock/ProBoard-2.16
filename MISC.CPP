#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <stdarg.h>
#include <ctype.h>
#include <string.h>
#include <tswin.hpp>
#include "proboard.hpp"
#include "fossil.hpp"

void
logentry(char *data)
{
    LOG("%s",data);
}

static void near
dolog(int level,char *str)
{
    if((!io.baud && !cfg.loglocal) || user.logLevel<level) return;

    Date date(TODAY);
    Time time(NOW);

    File fp("PROBOARD.LOG",fmode_write | fmode_text | fmode_copen | fmode_append);
    fp.seek(fp.len());

    fp.printf("%02d-%s-%02d %02d:%02d:%02d %s\n",date[0],
                                                months_short[date[1]],
                                                date[2],
                                                time[0],time[1],time[2],
                                                str);
}


void
LOG(char *str ...)
{
    char s[200];
    va_list va;
    va_start(va,str);

    vsprintf(s,str,va);

    dolog(0,s);
}

void
LOG(int level,char *str ...)
{
    char s[200];
    va_list va;
    va_start(va,str);

    vsprintf(s,str,va);

    dolog(level,s);
}

void
fatalerror(char *s)
{
    String str("Fatal error: ");
    str << s;
    LOG("%s",(char *)str);

    if(io.baud > 0)
    {
        io << "\n\n" << S_FATAL_ERROR;
    }
    else
    {
        SCREEN << "\f\n\n" << str << '\n';
    }

    fatal = TRUE;

    exit(ERRLVL_FATALERR);
}

int
linecounter(int reset)  // reset=-1 -> increment
{
    static int linecount=0;
    static bool continuous = FALSE;

    if(reset>=0)
      {
         linecount = reset;
         continuous = FALSE;
         return 0;
      }

    if(++linecount>=((/*rip_mode ? rip_screenlines:*/user.screenLength)-1) && !continuous && pause_mode)
    {
        char rep;

        if(io.more_func)
        {
         rep = char((*io.more_func)());
        }
        else
        {
         byte oldcolor=SCREEN.attrib();

         /*
         if(rip_mode)
         {
            rip_show_more();
            io.show_remote = FALSE;
         }
         */

         io << S_MORE;

         int prompt_len = strlen(S_MORE);

         rep = wait_language_hotkeys(K_MORE);

         io << '\r' << String(' ',prompt_len) << '\r';

         io.fullcolor(oldcolor);

         io.show_remote = TRUE;

         /*
         if(rip_mode)
         {
            rip_clear_status();
            rip_show_enter();
         }
         */
        }

        if(rep == '\r') rep = 0;
        if(rep == 2)
        {
         continuous = TRUE;
         rep = 0;
        }

        linecount=0;

        return rep;
    }

    return 0;
}

int
check_access(unsigned level,AccessFlags flags)
{
    if(user.level<level) return 0;
    for(int i=1;i<=32;i++) if(flags.getflag(i)) if(!user.aFlags.getflag(i)) return 0;
    return 1;
}

int
check_access(unsigned level,AccessFlags flags,AccessFlags notflags)
{
    if(user.level<level) return 0;
    for(int i=1;i<=32;i++) if(   flags.getflag(i)) if(!user.aFlags.getflag(i)) return 0;
    for(    i=1;i<=32;i++) if(notflags.getflag(i)) if(user.aFlags.getflag(i) ) return 0;

    return 1;
}


char *
strstrl(char *s,char *f)
{
    int lf=strlen(f);
    int l=strlen(s)-lf+1;

    for(int i=0;i<l;i++) if(!memicmp(&s[i],f,lf)) return &s[i];
    return NULL;
}

int
matchstring(char *s,char *d)
{
   for(;*s && *d;s++,d++)
   {
      switch(*s)
      {
         case '?': continue;
         case '*': return 1;
         default : if(toupper(*s)!=toupper(*d)) return 0;
      }
   }
   if(toupper(*s) != toupper(*d) && *s!='*')
      return 0;
   else
      return 1;
}

int
matchfile(char *search,char *file)
{
   String sbody,sext,fbody,fext;

   while(*search!='.' && *search)
      sbody << *search++;

   if(!*search++) return 0;

   while(*search)
      sext << *search++;

   while(*file!='.' && *file)
      fbody << *file++;

   if(!*file++) return 0;

   while(*file)
      fext << *file++;

   if(matchstring(sbody,fbody) && matchstring(sext,fext))
      return 1;

   return 0;
}


void
strip_path(char *s)
{
    int l=strlen(s);

    for(int i=l-1;i>=0;i--)
    {
        if(s[i]=='\\' || s[i]=='/') break;
    }

    if(i>=0) memmove(s,&s[i+1],l-i);
}

void
strip_fname(char *s)
{
    int l=strlen(s);

    for(int i=l-1;i>=0;i--)
    if(s[i]=='\\' || s[i]=='/') { s[i+1]='\0'; break; }
}

int
parse_data(char *s,String *param)
{
   for(int i=0;;i++)
   {
      int inquote=0;

      while(*s==' ' && *s) s++;

      if(*s=='\0') break;

      param[i].clear();

      while(*s)
      {
         if(*s==' ' && !inquote) break;

         if(*s=='\"')
         {
            inquote = !inquote;
            s++;
            continue;
         }

         param[i] << (*s++);
      }
   }

   return i;
}

void
adjust_limits()
{
    long kbgranted;
    int i,j;

    for(i=0;i<num_limits-1;i++)
    for(j=i+1;j<num_limits;j++)
        if(limit[i].level<limit[j].level)
        {
        limits temp;
        temp     = limit[i];
        limit[i] = limit[j];
        limit[j] = temp;
        }

    for(int n=-1;;)
    {
    for(i=num_limits-1;i>=0;i--) if(user.level>=limit[i].level) n=i;

    if(n<0) return;

    if(limit[n].max_download && user.kbDownloaded>limit[n].max_download)
        {
        if(user.level != limit[n].fallto)
            {
            user.level = limit[n].fallto;
            continue;
            }
        }

    if(limit[n].factor)
        {
        kbgranted =   long(user.kbUploaded)*100L/limit[n].factor
                    + long(user.msgsPosted) * limit[n].msgfactor
                    + limit[n].free;
        }
        else kbgranted=0x7FFFFFFFL;

    if(user.kbDownloaded>kbgranted)
        {
        if(!limit[n].factor)
            upload_needed = 0x7FFF;
        else
            upload_needed=int((long(user.kbDownloaded)-kbgranted)*limit[n].factor/100);
        free_download=0;
        }
        else
        {
        free_download = int(kbgranted-user.kbDownloaded);
        upload_needed = 0;

        if(free_download>limit[n].daily_klimit) free_download=limit[n].daily_klimit;
        }

    break;
    }

    time_limit     = limit[n].timelimit;
    download_limit = limit[n].daily_klimit;
    download_delay = limit[n].pre_download;

    timer.changeleft(time_limit-timer.used()-user.timeUsed-time_removed);

    updatemenu = TRUE;
}

void
change_access(char *data)
{
    String param[40];

    int n = parse_data(data,param);

    for(int i=0;i<n;i++)
    {
      param[i].upperCase();

      if(isalpha(param[i][0]) || (param[i][0]>='1' && param[i][0]<='6' && strchr("+-",param[i][1])))
      {
         int flag;

         if(isalpha(param[i][0]))
            flag = param[i][0]-64;
         else
            flag = param[i][0]-'1'+27;

         if(param[i][1] == '-')
            user.aFlags.clearflag(flag);
         else
            user.aFlags.setflag(flag);
        }
        else
        {
            user.level = word(atol(param[i]));
        }
    }

   adjust_limits();
   timer.check();

   updatemenu = TRUE;
}

void
file_error(int fn , int errno)
{
   String s("Error accessing file ");

   switch(abs(fn))
   {
      case ERR_BINLOG_PB   : s << FN_BINLOG_PB;    break;
      case ERR_TIMELOG_PRO : s << FN_TIMELOG_PRO;  break;
      case ERR_ONLINE_PRO  : s << FN_ONLINE_PRO;   break;
      case ERR_AKA_PRO     : s << "AKA.PRO";       break;
   }

   if(errno)
      s << form(" (%d - %s)", errno , dos_error_message(errno));

   if(fn<0)
      LOG("%s",(char *)s);
   else
      fatalerror(s);
}

void
file_error(char *fn , int errno)
{
   String s("Error accessing file ");

   s << fn;

   if(errno) s << form(" (%d - %s)", errno , dos_error_message(errno));

   fatalerror(s);
}

void
file_warning(char *fn , int errno)
{
    String s("Error accessing file ");

    s << fn;

    if(errno)
      s << form(" (%d - %s)", errno , dos_error_message(errno));

    LOG(s);
}


int
intimewindow(Time t1,Time t2)
{
   Time now(NOW);

   if(t2 <= t1)
      t2[0] += 24;

   if(now<t1)
      now[0] += 24;

   if(now<t1 || now>t2)
      return 0;
   else
      return 1;
}

char *
dos_error_message(int errno)
{
 static struct { int no;
          char *msg;
        } errmsg[] = { { 1  , "Invalid DOS call (SHARE not loaded?)" } ,
                        { 2  , "File not found"                       } ,
                        { 3  , "Directory not found"                  } ,
                        { 4  , "Too many open files"                  } ,
                        { 5  , "Access denied"                        } ,
                        { 6  , "Invalid handle"                       } ,
                        { 7  , "MCB destroyed"                        } ,
                        { 8  , "Not enough memory"                    } ,
                        { 0  , ""                                     }
                      };

 for(int i=0;errmsg[i].no;i++)
   {
    if(errno == errmsg[i].no) break;
   }

 return errmsg[i].msg;
}

void
parse_date(Date& d , char *s , int format)
{
   char str[9];

   strncpy(str,s,8);
   str[8] = '\0';

   char *fmt = date_formats[format];

   int fmt_ar[3];

   if(toupper(fmt[0] == 'D')) fmt_ar[0] = 0;
   if(toupper(fmt[0] == 'M')) fmt_ar[0] = 1;
   if(toupper(fmt[0] == 'Y')) fmt_ar[0] = 2;
   if(toupper(fmt[3] == 'D')) fmt_ar[1] = 0;
   if(toupper(fmt[3] == 'M')) fmt_ar[1] = 1;
   if(toupper(fmt[3] == 'Y')) fmt_ar[1] = 2;
   if(toupper(fmt[6] == 'D')) fmt_ar[2] = 0;
   if(toupper(fmt[6] == 'M')) fmt_ar[2] = 1;
   if(toupper(fmt[6] == 'Y')) fmt_ar[2] = 2;

   d.set(0,0,0);

   s = strtok(str, "-/.");
   if(s)
   {
      d[fmt_ar[0]] = atoi(s);
      s = strtok(NULL, "-/.");
      d[fmt_ar[1]] = atoi(s);
      s = strtok(NULL, "-/.");
      d[fmt_ar[2]] = atoi(s);
   }
}

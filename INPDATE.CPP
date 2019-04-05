#include <ctype.h>
#include <tswin.hpp>
#include "proboard.hpp"

void
InputDate(Date& d)
{
   char s[9];
   byte k;

   io.drawfield(8);

   int i=0;
   for(;;)
   {
      k=io.wait();

      if((!isdigit(k) && k!=13 && k!=8) || k>0x7F)
         continue;

      if(k==8)
      {
         if(i>0)
         {
            i--;
            io << "\b \b";
            if(i==2 || i==5)
            {
               i--;
               io << "\b \b";
            }
         }

         continue;
      }

      if(k==13)
      {
         if(i==0)
         {
            d.set(0,0,0);
            break;
         }

         if(i!=8)
            continue;
         else
         {
            parse_date(d,s,user.dateFormat);
            if(d.ok())
               break;
            else
               continue;
         }

      }

      if(i==8)
         continue;

      s[i++] = k;
      io << k;

      if(i==2 || i==5)
      {
         s[i++] = '/';
         io << '/';
      }
   }

   io << '\r';
   s[i]='\0';

   if(avatar)
      io << char(22) << char(1) << char(SCREEN.attrib()&0x0F);
   else
      if(ansi_mode) io << "\x1b[40m";

   io.flush();
}

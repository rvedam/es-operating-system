// test setFullYear().

stdout = System.output;
function check(result)
{
    if (result)
    {
        stdout.write("OK\n", 3);
    }
    else
    {
        stdout.write("*** ERROR ***\n", 14);
    }
    return result;
}

d = new Date(2007, 7-1, 18, 0, 40, 59 /* localtime */);
s = d.toLocaleString(); // LocalTime
stdout.write(s + '\n', s.length + 1);

d.setUTCFullYear(2005);
s = d.toLocaleString();
check(s == "Mon Jul 18 00:40:59 2005");

d.setFullYear(2007, 6-1);
s = d.toLocaleString();
check(s == "Mon Jun 18 00:40:59 2007");

d.setFullYear(1999, 7-1, 30);
s = d.toLocaleString();
check(s == "Fri Jul 30 00:40:59 1999");

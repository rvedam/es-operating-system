// test toTimeString().
function check(result)
{
    stdout = System.getOut();
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
d = new Date(2007, 7-1, 18, 13, 20, 59, 30 /* localtime */);
s = d.toTimeString();
check(s == "13:20:59");

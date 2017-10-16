#ifndef ASSETS_H
#define ASSETS_H


class assets
{
public:
    static const char * getLogoPngBuf() { return (const char *) logoPngBuf; }
    static int getLogoPngLen() { return 5311; }

private:
    static unsigned int logoPngBuf[];
};

#endif // ASSETS_H

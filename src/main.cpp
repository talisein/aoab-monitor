#include <locale>

int main(int argc, char *argv[])
{
    std::locale::global(std::locale(""));
}

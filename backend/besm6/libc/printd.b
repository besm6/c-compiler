print_d(n) {
    auto a;

    if (n < 0) {
        writeb('-');
        n = -n;
    }
    if ((a = n / 10)) {
        print_d(a);
    }
    writeb(n - a*10 + '0');
}

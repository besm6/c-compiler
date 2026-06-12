print_o(n) {
    auto a;

    if ((a = n >> 3)) {
        print_o(a);
    }
    writeb((n & 7) + '0');
}

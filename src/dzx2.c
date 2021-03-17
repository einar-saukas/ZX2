/*
 * ZX2 decompressor - by Einar Saukas
 * https://github.com/einar-saukas/ZX2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 65536  /* must be > MAX_OFFSET */
#define INITIAL_OFFSET 1

#define FALSE 0
#define TRUE 1

FILE *ifp;
FILE *ofp;
char *input_name;
char *output_name;
unsigned char *input_data;
unsigned char *output_data;
size_t input_index;
size_t output_index;
size_t input_size;
size_t output_size;
size_t partial_counter;
int bit_mask;
int bit_value;
int backtrack;
int last_byte;
int last_offset;

int read_byte() {
    if (input_index == partial_counter) {
        input_index = 0;
        partial_counter = fread(input_data, sizeof(char), BUFFER_SIZE, ifp);
        input_size += partial_counter;
        if (partial_counter == 0) {
            fprintf(stderr, (input_size ? "Error: Truncated input file %s\n" : "Error: Empty input file %s\n"), input_name);
            exit(1);
        }
    }
    last_byte = input_data[input_index++];
    return last_byte;
}

int read_bit() {
    bit_mask >>= 1;
    if (bit_mask == 0) {
        bit_mask = 128;
        bit_value = read_byte();
    }
    return bit_value & bit_mask ? 1 : 0;
}

int read_interlaced_elias_gamma(int limited_length) {
    int value = 1;
    while (read_bit()) {
        value = value << 1 | read_bit();;
    }

    if (limited_length && value > 255) {
        fprintf(stderr, "Error: Large block length in input file %s\n", input_name);
        exit(1);
    }

    return value;
}

void save_output() {
    if (output_index != 0) {
        if (fwrite(output_data, sizeof(char), output_index, ofp) != output_index) {
            fprintf(stderr, "Error: Cannot write output file %s\n", output_name);
            exit(1);
        }
        output_size += output_index;
        output_index = 0;
    }
}

void write_byte(int value) {
    output_data[output_index++] = value;
    if (output_index == BUFFER_SIZE) {
        save_output();
    }
}

void write_bytes(int offset, int length) {
    int i;

    if (offset > output_size+output_index) {
        fprintf(stderr, "Error: Invalid data in input file %s\n", input_name);
        exit(1);
    }
    while (length-- > 0) {
        i = output_index-offset;
        write_byte(output_data[i >= 0 ? i : BUFFER_SIZE+i]);
    }
}

void decompress(int default_offset, int min_length, int limited_length) {
    int length;
    int i;

    input_data = (unsigned char *)malloc(BUFFER_SIZE);
    output_data = (unsigned char *)malloc(BUFFER_SIZE);
    if (!input_data || !output_data) {
        fprintf(stderr, "Error: Insufficient memory\n");
        exit(1);
    }

    input_size = 0;
    input_index = 0;
    partial_counter = 0;
    output_index = 0;
    output_size = 0;
    bit_mask = 0;
    backtrack = FALSE;
    last_offset = default_offset;

COPY_LITERALS:
    length = read_interlaced_elias_gamma(limited_length);
    for (i = 0; i < length; i++) {
        write_byte(read_byte());
    }
    if (read_bit()) {
        goto COPY_FROM_NEW_OFFSET;
    }

/*COPY_FROM_LAST_OFFSET:*/
    length = read_interlaced_elias_gamma(limited_length);
    write_bytes(last_offset, length);
    if (!read_bit()) {
        goto COPY_LITERALS;
    }

COPY_FROM_NEW_OFFSET:
    last_offset = 255-read_byte();                   
    if (!last_offset) {
        save_output();
        if (input_index != partial_counter) {
            fprintf(stderr, "Error: Input file %s too long\n", input_name);
            exit(1);
        }
        return;
    }
    length = read_interlaced_elias_gamma(limited_length)+min_length-1;
    write_bytes(last_offset, length);
    if (read_bit()) {
        goto COPY_FROM_NEW_OFFSET;
    } else {
        goto COPY_LITERALS;
    }
}

int main(int argc, char *argv[]) {
    int forced_mode = FALSE;
    int default_offset = INITIAL_OFFSET;
    int min_length = 2;
    int limited_length = FALSE;
    int i;

    printf("DZX2 v1.0: Data decompressor by Einar Saukas\n");
    
    /* process hidden optional parameters */
    for (i = 1; i < argc && *argv[i] == '-'; i++) {
        if (!strcmp(argv[i], "-f")) {
            forced_mode = TRUE;
        } else if (!strcmp(argv[i], "-z")) {
            default_offset = 0;
        } else if (!strcmp(argv[i], "-x")) {
            min_length = 1;
        } else if (!strcmp(argv[i], "-y")) {
            limited_length = TRUE;
        } else {
            fprintf(stderr, "Error: Invalid parameter %s\n", argv[i]);
            exit(1);
        }
    }

    /* determine output filename */
    if (argc == i+1) {
        input_name = argv[i];
        input_size = strlen(input_name);
        if (input_size > 4 && !strcmp(input_name+input_size-4, ".zx2")) {
            input_size = strlen(input_name);
            output_name = (char *)malloc(input_size);
            strcpy(output_name, input_name);
            output_name[input_size-4] = '\0';
        } else {
            fprintf(stderr, "Error: Cannot infer output filename\n");
            exit(1);
        }
    } else if (argc == i+2) {
        input_name = argv[i];
        output_name = argv[i+1];
    } else {
        fprintf(stderr, "Usage: %s [-f] [-z] [-x] [-y] input.zx2 [output]\n"
                        "  -f      Force overwrite of output file\n"
                        "  -z      Ignore default offset\n"
                        "  -x      Skip length increment\n"
                        "  -y      Limit block length\n", argv[0]);
        exit(1);
    }

    /* open input file */
    ifp = fopen(input_name, "rb");
    if (!ifp) {
        fprintf(stderr, "Error: Cannot access input file %s\n", input_name);
        exit(1);
    }

    /* check output file */
    if (!forced_mode && fopen(output_name, "rb") != NULL) {
        fprintf(stderr, "Error: Already existing output file %s\n", output_name);
        exit(1);
    }

    /* create output file */
    ofp = fopen(output_name, "wb");
    if (!ofp) {
        fprintf(stderr, "Error: Cannot create output file %s\n", output_name);
        exit(1);
    }

    /* generate output file */
    decompress(default_offset, min_length, limited_length);

    /* close input file */
    fclose(ifp);

    /* close output file */
    fclose(ofp);

    /* done! */
    printf("File decompressed from %lu to %lu bytes!\n", (unsigned long)input_size, (unsigned long)output_size);

    return 0;
}
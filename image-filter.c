#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include "timer.h"

typedef struct
{
    int id;
    int start_row;
    int end_row;
    uint8_t ***input;
    uint8_t ***output;
    int width;
    int height;
} ThreadData;

// Aloca matriz 3D (altura x largura x 3)
uint8_t ***alloc_image(int h, int w)
{
    uint8_t ***img = malloc(h * sizeof(uint8_t **));
    for (int y = 0; y < h; y++)
    {
        img[y] = malloc(w * sizeof(uint8_t *));
        for (int x = 0; x < w; x++)
        {
            img[y][x] = calloc(3, sizeof(uint8_t));
        }
    }
    return img;
}

void free_image(uint8_t ***img, int h, int w)
{
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            free(img[y][x]);
        }
        free(img[y]);
    }
    free(img);
}

void load_image(uint8_t ***image, const char *path, int *w, int *h)
{
    int channels;
    uint8_t *data = stbi_load(path, w, h, &channels, 3);
    if (!data)
    {
        fprintf(stderr, "Erro ao carregar imagem.\n");
        exit(1);
    }

    for (int y = 0; y < *h; y++)
    {
        for (int x = 0; x < *w; x++)
        {
            for (int c = 0; c < 3; c++)
            {
                image[y][x][c] = data[(y * (*w) + x) * 3 + c];
            }
        }
    }
    stbi_image_free(data);
}

void save_image(uint8_t ***image, int w, int h, const char *path)
{
    uint8_t *data = malloc(w * h * 3);
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            for (int c = 0; c < 3; c++)
            {
                data[(y * w + x) * 3 + c] = image[y][x][c];
            }
        }
    }
    stbi_write_png(path, w, h, 3, data, w * 3);
    free(data);
}

// Função que será executada por cada thread
void apply_color_filter(uint8_t ***image, int start_row, int end_row, int width)
{
    for (int y = start_row; y < end_row; y++)
    {
        for (int x = 0; x < width; x++)
        {
            image[y][x][1] = 0; // Remove verde
            image[y][x][2] = 0; // Remove azul
        }
    }
}
void apply_laplacian_block(uint8_t ***input, uint8_t ***output, int start_row, int end_row, int width, int height)
{
    int kernel[3][3] = {
        {0, -1, 0},
        {-1, 4, -1},
        {0, -1, 0}};

    for (int y = start_row; y < end_row; y++)
    {
        if (y == 0 || y == height - 1)
            continue; // evita borda
        for (int x = 1; x < width - 1; x++)
        {
            for (int c = 0; c < 3; c++)
            {
                int sum = 0;
                for (int ky = -1; ky <= 1; ky++)
                {
                    for (int kx = -1; kx <= 1; kx++)
                    {
                        sum += kernel[ky + 1][kx + 1] * input[y + ky][x + kx][c];
                    }
                }
                int value = input[y][x][c] + sum;
                if (value < 0)
                    value = 0;
                if (value > 255)
                    value = 255;
                output[y][x][c] = (uint8_t)value;
            }
        }
    }
}
void *thread_func(void *arg)
{
    ThreadData *data = (ThreadData *)arg;

    // Filtro de cor
    apply_color_filter(data->input, data->start_row, data->end_row, data->width);

    // Filtro Laplaciano
    apply_laplacian_block(data->input, data->output, data->start_row, data->end_row, data->width, data->height);

    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Uso: %s <input_image> <output_image> <num_threads>\n", argv[0]);
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = argv[2];
    int num_threads = atoi(argv[3]);

    if (num_threads <= 0)
    {
        fprintf(stderr, "Número inválido de threads.\n");
        return 1;
    }

    int width, height;
    double ini, fim, delta;
    GET_TIME(ini);

    uint8_t ***input_image = alloc_image(3000, 3000); // tamanho inicial arbitrário
    load_image(input_image, input_path, &width, &height);
    printf("Imagem carregada: %d x %d\n", width, height);

    uint8_t ***output_image = alloc_image(height, width);

    pthread_t threads[num_threads];
    ThreadData thread_data[num_threads];
    int rows_per_thread = height / num_threads;

    for (int i = 0; i < num_threads; i++)
    {
        thread_data[i].id = i;
        thread_data[i].start_row = i * rows_per_thread;
        thread_data[i].end_row = (i == num_threads - 1) ? height : (i + 1) * rows_per_thread;
        thread_data[i].input = input_image;
        thread_data[i].output = output_image;
        thread_data[i].width = width;
        thread_data[i].height = height;

        pthread_create(&threads[i], NULL, thread_func, &thread_data[i]);
    }

    for (int i = 0; i < num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }

    save_image(output_image, width, height, output_path);
    printf("Imagem com filtro salva em: %s\n", output_path);

    free_image(input_image, height, width);
    free_image(output_image, height, width);

    GET_TIME(fim);
    delta = fim - ini;
    printf("Tempo de execução: %f segundos\n", delta);

    return 0;
}

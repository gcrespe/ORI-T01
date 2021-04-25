/* ==========================================================================
 * Universidade Federal de São Carlos - Campus Sorocaba
 * Disciplina: Organização de Recuperação da Informação
 * Prof. Tiago A. Almeida
 *
 * Trabalho 01 - Indexação
 *
 * RA: 743543
 * Aluno: Giuliano Crespe
 * ========================================================================== */

/* Bibliotecas */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>
#include <string.h>

/* Tamanho dos campos dos registros */
/* Campos de tamanho fixo */
#define TAM_CPF 12
#define TAM_NASCIMENTO 11
#define TAM_CELULAR 12
#define TAM_SALDO 14
#define TAM_TIMESTAMP 15

/* Campos de tamanho variável (tamanho máximo) */
// Restaram 204 bytes (48B nome, 47B email, 11B chave CPF, 11B chave número de celular,
// 47B chave email, 37B chave aleatória, 3B demilitadores)
#define TAM_MAX_NOME 48
#define TAM_MAX_EMAIL 47
#define TAM_MAX_CHAVE_PIX 48

#define MAX_REGISTROS 1000
#define TAM_REGISTRO_CLIENTE 256
#define TAM_REGISTRO_TRANSACAO 49
#define TAM_ARQUIVO_CLIENTE (TAM_REGISTRO_CLIENTE * MAX_REGISTROS + 1)
#define TAM_ARQUIVO_TRANSACAO (TAM_REGISTRO_TRANSACAO * MAX_REGISTROS + 1)

/* Mensagens padrões */
#define SUCESSO                          "OK\n"
#define AVISO_NENHUM_REGISTRO_ENCONTRADO "AVISO: Nenhum registro encontrado\n"
#define ERRO_OPCAO_INVALIDA              "ERRO: Opcao invalida\n"
#define ERRO_MEMORIA_INSUFICIENTE        "ERRO: Memoria insuficiente\n"
#define ERRO_PK_REPETIDA                 "ERRO: Ja existe um registro com a chave primaria %s\n"
#define ERRO_REGISTRO_NAO_ENCONTRADO     "ERRO: Registro nao encontrado\n"
#define ERRO_SALDO_NAO_SUFICIENTE        "ERRO: Saldo insuficiente\n"
#define ERRO_VALOR_INVALIDO              "ERRO: Valor invalido\n"
#define ERRO_CHAVE_PIX_REPETIDA          "ERRO: Ja existe uma chave PIX %s\n"
#define ERRO_CHAVE_PIX_DUPLICADA         "ERRO: Chave PIX tipo %c ja cadastrada para este usuario\n"
#define ERRO_CHAVE_PIX_INVALIDA          "ERRO: Chave PIX invalida\n"
#define ERRO_TIPO_PIX_INVALIDO           "ERRO: Tipo %c invalido para chaves PIX\n"
#define ERRO_ARQUIVO_VAZIO               "ERRO: Arquivo vazio\n"
#define ERRO_NAO_IMPLEMENTADO            "ERRO: Funcao %s nao implementada\n"

/* Registro de Cliente */
typedef struct {
    char cpf[TAM_CPF];
    char nome[TAM_MAX_NOME];
    char nascimento[TAM_NASCIMENTO];
    char email[TAM_MAX_EMAIL];
    char celular[TAM_CELULAR];
    double saldo;
    char chaves_pix[4][TAM_MAX_CHAVE_PIX];
} Cliente;

/* Registro de transação */
typedef struct {
    char cpf_origem[TAM_CPF];
    char cpf_destino[TAM_CPF];
    double valor;
    char timestamp[TAM_TIMESTAMP];
} Transacao;

/*----- Registros dos índices -----*/

/* Struct para índice primário de clientes */
typedef struct {
    char cpf[TAM_CPF];
    int rrn;
} clientes_index;

/* Struct para índice primário de transações */
typedef struct {
    char cpf_origem[TAM_CPF];
    char timestamp[TAM_TIMESTAMP];
    int rrn;
} transacoes_index;

/* Struct para índice secundário de chaves PIX */
typedef struct {
    char chave_pix[TAM_MAX_CHAVE_PIX];
    char pk_cliente[TAM_CPF];
} chaves_pix_index;

/* Struct para índice secundário de timestamp e CPF origem */
typedef struct {
    char timestamp[TAM_TIMESTAMP];
    char cpf_origem[TAM_CPF];
} timestamp_cpf_origem_index;

/* Variáveis globais */
/* Arquivos de dados */
char ARQUIVO_CLIENTES[TAM_ARQUIVO_CLIENTE];
char ARQUIVO_TRANSACOES[TAM_ARQUIVO_TRANSACAO];

/* Índices */
clientes_index *clientes_idx = NULL;
transacoes_index *transacoes_idx = NULL;
chaves_pix_index *chaves_pix_idx = NULL;
timestamp_cpf_origem_index *timestamp_cpf_origem_idx = NULL;

/* Contadores */
unsigned qtd_registros_clientes = 0;
unsigned qtd_registros_transacoes = 0;
unsigned qtd_chaves_pix = 0;

/* Funções de geração determinística de números pseudo-aleatórios */
uint64_t prng_seed;

void prng_srand(uint64_t value) {
    prng_seed = value;
}

uint64_t prng_rand() {
    // https://en.wikipedia.org/wiki/Xorshift#xorshift*
    uint64_t x = prng_seed; // O estado deve ser iniciado com um valor diferente de 0
    x ^= x >> 12; // a
    x ^= x << 25; // b
    x ^= x >> 27; // c
    prng_seed = x;
    return x * UINT64_C(0x2545F4914F6CDD1D);
}

/**
 * Gera um <a href="https://en.wikipedia.org/wiki/Universally_unique_identifier">UUID Version-4 Variant-1</a>
 * (<i>string</i> aleatória) de 36 caracteres utilizando o gerador de números pseudo-aleatórios
 * <a href="https://en.wikipedia.org/wiki/Xorshift#xorshift*">xorshift*</a>. O UUID é
 * escrito na <i>string</i> fornecida como parâmetro.<br />
 * <br />
 * Exemplo de uso:<br />
 * <code>
 * char chave_aleatoria[37];<br />
 * new_uuid(chave_aleatoria);<br />
 * printf("chave aleatória: %s&#92;n", chave_aleatoria);<br />
 * </code>
 *
 * @param buffer String de tamanho 37 no qual será escrito
 * o UUID. É terminado pelo caractere <code>\0</code>.
 */
void new_uuid(char buffer[37]) {
    uint64_t r1 = prng_rand();
    uint64_t r2 = prng_rand();

    sprintf(buffer, "%08x-%04x-%04llx-%04llx-%012llx", (uint32_t)(r1 >> 32), (uint16_t)(r1 >> 16), 0x4000 | (r1 & 0x0fff), 0x8000 | (r2 & 0x3fff), r2 >> 16);
}

/* Funções de manipulação de data (timestamp) */
int64_t epoch;

void set_time(int64_t value) {
    epoch = value;
}

void tick_time() {
    epoch += prng_rand() % 864000; // 10 dias
}

struct tm gmtime_(const int64_t lcltime) {
    // based on https://sourceware.org/git/?p=newlib-cygwin.git;a=blob;f=newlib/libc/time/gmtime_r.c;
    struct tm res;
    long days = lcltime / 86400 + 719468;
    long rem = lcltime % 86400;
    if (rem < 0) {
        rem += 86400;
        --days;
    }

    res.tm_hour = (int) (rem / 3600);
    rem %= 3600;
    res.tm_min = (int) (rem / 60);
    res.tm_sec = (int) (rem % 60);

    int weekday = (3 + days) % 7;
    if (weekday < 0) weekday += 7;
    res.tm_wday = weekday;

    int era = (days >= 0 ? days : days - 146096) / 146097;
    unsigned long eraday = days - era * 146097;
    unsigned erayear = (eraday - eraday / 1460 + eraday / 36524 - eraday / 146096) / 365;
    unsigned yearday = eraday - (365 * erayear + erayear / 4 - erayear / 100);
    unsigned month = (5 * yearday + 2) / 153;
    unsigned day = yearday - (153 * month + 2) / 5 + 1;
    month += month < 10 ? 2 : -10;

    int isleap = ((erayear % 4) == 0 && (erayear % 100) != 0) || (erayear % 400) == 0;
    res.tm_yday = yearday >= 306 ? yearday - 306 : yearday + 59 + isleap;
    res.tm_year = (erayear + era * 400 + (month <= 1)) - 1900;
    res.tm_mon = month;
    res.tm_mday = day;
    res.tm_isdst = 0;

    return res;
}

/**
 * Escreve a <i>timestamp</i> atual no formato <code>AAAAMMDDHHmmSS</code> em uma <i>string</i>
 * fornecida como parâmetro.<br />
 * <br />
 * Exemplo de uso:<br />
 * <code>
 * char timestamp[TAM_TIMESTAMP];<br />
 * current_timestamp(timestamp);<br />
 * printf("timestamp atual: %s&#92;n", timestamp);<br />
 * </code>
 *
 * @param buffer String de tamanho <code>TAM_TIMESTAMP</code> no qual será escrita
 * a <i>timestamp</i>. É terminado pelo caractere <code>\0</code>.
 */
void current_timestamp(char buffer[TAM_TIMESTAMP]) {
    // http://www.cplusplus.com/reference/ctime/strftime/
    // http://www.cplusplus.com/reference/ctime/gmtime/
    // AAAA MM DD HH mm SS
    // %Y   %m %d %H %M %S
    struct tm tm_ = gmtime_(epoch);
    strftime(buffer, TAM_TIMESTAMP, "%Y%m%d%H%M%S", &tm_);
}

/* Remove comentários (--) e caracteres whitespace do começo e fim de uma string */
void clear_input(char *str) {
    char *ptr = str;
    int len = 0;

    for (; ptr[len]; ++len) {
        if (strncmp(&ptr[len], "--", 2) == 0) {
            ptr[len] = '\0';
            break;
        }
    }

    while(len-1 > 0 && isspace(ptr[len-1]))
        ptr[--len] = '\0';

    while(*ptr && isspace(*ptr))
        ++ptr, --len;

    memmove(str, ptr, len + 1);
}

/* ==========================================================================
 * ========================= PROTÓTIPOS DAS FUNÇÕES =========================
 * ========================================================================== */

/* (Re)faz o índice respectivo */
void criar_clientes_index();
void criar_transacoes_index();
void criar_chaves_pix_index();
void criar_timestamp_cpf_origem_index();

/* Exibe um registro */
int exibir_cliente(int rrn);
int exibir_transacao(int rrn);

/* Recupera do arquivo o registro com o RRN informado
 * e retorna os dados nas struct Cliente/Transacao */
Cliente recuperar_registro_cliente(int rrn);
Transacao recuperar_registro_transacao(int rrn);

/* Funções principais */
void cadastrar_cliente(char *cpf, char *nome, char *nascimento, char *email, char *celular);
void remover_cliente(char *cpf);
void alterar_saldo(char *cpf, double valor);
void cadastrar_chave_pix(char *cpf, char tipo);
void transferir(char *chave_pix_origem, char *chave_pix_destino, double valor);

/* Busca */
void buscar_cliente_cpf(char *cpf);
void buscar_cliente_chave_pix(char *chave_pix);
void buscar_transacao_cpf_origem_timestamp(char *cpf, char *timestamp);

/* Listagem */
void listar_clientes_cpf();
void listar_transacoes_periodo(char *data_inicio, char *data_fim);
void listar_transacoes_cpf_origem_periodo(char *cpf, char *data_inicio, char *data_fim);

/* Liberar espaço */
void liberar_espaco();

/* Imprimir arquivos de dados */
void imprimir_arquivo_clientes();
void imprimir_arquivo_transacoes();

/* Imprimir índices secundários */
void imprimir_chaves_pix_index();
void imprimir_timestamp_cpf_origem_index();

/* Liberar memória e encerrar programa */
void liberar_memoria();

/* <<< COLOQUE AQUI OS DEMAIS PROTÓTIPOS DE FUNÇÕES, SE NECESSÁRIO >>> */

int existe_chave(char *chave_pix, char *cpf);
void escrever_registro_cliente(Cliente c, int rrn);
void escrever_arquivo_transacoes(Transacao t, int rrn);
int binarySearchFloor(char *data_fim);
int binarySearchCeiling(char *data_inicio);
int binarySearchFloor2(char *data_inicio, char *cpf);
int binarySearchCeiling2(char *data_fim, char *cpf);

/* Funções auxiliares para o qsort.
 * Com uma pequena alteração, é possível utilizá-las no bsearch, assim, evitando código duplicado.
 * */
int qsort_clientes_index(const void *a, const void *b);
int qsort_transacoes_index(const void *a, const void *b);
int qsort_chaves_pix_index(const void *a, const void *b);
int qsort_timestamp_cpf_origem_index(const void *a, const void *b);

/* ==========================================================================
 * ============================ FUNÇÃO PRINCIPAL ============================
 * =============================== NÃO ALTERAR ============================== */

int main() {
    // variáveis utilizadas pelo interpretador de comandos
    char input[500];
    uint64_t seed = 2;
    uint64_t time = 1616077800; // UTC 18/03/2021 14:30:00
    char cpf[TAM_CPF];
    char nome[TAM_MAX_NOME];
    char nascimento[TAM_NASCIMENTO];
    char email[TAM_MAX_EMAIL];
    char celular[TAM_CELULAR];
    double valor;
    char tipo_chave_pix;
    char chave_pix_origem[TAM_MAX_CHAVE_PIX];
    char chave_pix_destino[TAM_MAX_CHAVE_PIX];
    char chave_pix[TAM_MAX_CHAVE_PIX];
    char timestamp[TAM_TIMESTAMP];
    char data_inicio[TAM_TIMESTAMP];
    char data_fim[TAM_TIMESTAMP];

    scanf("SET ARQUIVO_CLIENTES '%[^\n]\n", ARQUIVO_CLIENTES);
    int temp_len = strlen(ARQUIVO_CLIENTES);
    if (temp_len < 2) temp_len = 2; // corrige o tamanho caso a entrada seja omitida
    qtd_registros_clientes = (temp_len - 2) / TAM_REGISTRO_CLIENTE;
    ARQUIVO_CLIENTES[temp_len - 2] = '\0';

    scanf("SET ARQUIVO_TRANSACOES '%[^\n]\n", ARQUIVO_TRANSACOES);
    temp_len = strlen(ARQUIVO_TRANSACOES);
    if (temp_len < 2) temp_len = 2; // corrige o tamanho caso a entrada seja omitida
    qtd_registros_transacoes = (temp_len - 2) / TAM_REGISTRO_TRANSACAO;
    ARQUIVO_TRANSACOES[temp_len - 2] = '\0';

    // inicialização do gerador de números aleatórios e função de datas
    prng_srand(seed);
    set_time(time);

    criar_clientes_index();
    criar_transacoes_index();
    criar_chaves_pix_index();
    criar_timestamp_cpf_origem_index();

    while (1) {
        fgets(input, 500, stdin);
        clear_input(input);

        /* Funções principais */
        if (sscanf(input, "INSERT INTO clientes VALUES ('%[^']', '%[^']', '%[^']', '%[^']', '%[^']');", cpf, nome, nascimento, email, celular) == 5)
            cadastrar_cliente(cpf, nome, nascimento, email, celular);
        else if (sscanf(input, "DELETE FROM clientes WHERE cpf = '%[^']';", cpf) == 1)
            remover_cliente(cpf);
        else if (sscanf(input, "UPDATE clientes SET saldo = saldo + %lf WHERE cpf = '%[^']';", &valor, cpf) == 2)
            alterar_saldo(cpf, valor);
        else if (sscanf(input, "UPDATE clientes SET chaves_pix = array_append(chaves_pix, '%c') WHERE cpf = '%[^']';", &tipo_chave_pix, cpf) == 2)
            cadastrar_chave_pix(cpf, tipo_chave_pix);
        else if (sscanf(input, "INSERT INTO transacoes VALUES ('%[^']', '%[^']', %lf);", chave_pix_origem, chave_pix_destino, &valor) == 3)
            transferir(chave_pix_origem, chave_pix_destino, valor);

        /* Busca */
        else if (sscanf(input, "SELECT * FROM clientes WHERE cpf = '%[^']';", cpf) == 1)
            buscar_cliente_cpf(cpf);
        else if (sscanf(input, "SELECT * FROM clientes WHERE '%[^']' = ANY (chaves_pix);", chave_pix) == 1)
            buscar_cliente_chave_pix(chave_pix);
        else if (sscanf(input, "SELECT * FROM transacoes WHERE cpf_origem = '%[^']' AND timestamp = '%[^']';", cpf, timestamp) == 2)
            buscar_transacao_cpf_origem_timestamp(cpf, timestamp);

        /* Listagem */
        else if (strcmp("SELECT * FROM clientes ORDER BY cpf ASC;", input) == 0)
            listar_clientes_cpf();
        else if (sscanf(input, "SELECT * FROM transacoes WHERE timestamp BETWEEN '%[^']' AND '%[^']' ORDER BY timestamp ASC;", data_inicio, data_fim) == 2)
            listar_transacoes_periodo(data_inicio, data_fim);
        else if (sscanf(input, "SELECT * FROM transacoes WHERE cpf_origem = '%[^']' AND timestamp BETWEEN '%[^']' AND '%[^']' ORDER BY timestamp ASC;", cpf, data_inicio, data_fim) == 3)
            listar_transacoes_cpf_origem_periodo(cpf, data_inicio, data_fim);

        /* Liberar espaço */
        else if (strcmp("VACUUM clientes;", input) == 0)
            liberar_espaco();

        /* Imprimir arquivos de dados */
        else if (strcmp("\\echo file ARQUIVO_CLIENTES", input) == 0)
            imprimir_arquivo_clientes();
        else if (strcmp("\\echo file ARQUIVO_TRANSACOES", input) == 0)
            imprimir_arquivo_transacoes();

        /* Imprimir índices secundários */
        else if (strcmp("\\echo index chaves_pix_index", input) == 0)
            imprimir_chaves_pix_index();
        else if (strcmp("\\echo index timestamp_cpf_origem_index", input) == 0)
            imprimir_timestamp_cpf_origem_index();

        /* Liberar memória e encerrar programa */
        else if (strcmp("\\q", input) == 0)
            { liberar_memoria(); return 0; }
        else if (sscanf(input, "SET SRAND %lu;", &seed) == 1)
            { prng_srand(seed); printf(SUCESSO); continue; }
        else if (sscanf(input, "SET TIME %lu;", &time) == 1)
            { set_time(time); printf(SUCESSO); continue; }
        else if (strcmp("", input) == 0)
            continue; // não avança o tempo caso seja um comando em branco
        else
            printf(ERRO_OPCAO_INVALIDA);

        tick_time();
    }
}

/* (Re)faz o índice primário clientes_index */
void criar_clientes_index() {
    if (!clientes_idx)
        clientes_idx = malloc(MAX_REGISTROS * sizeof(clientes_index));

    if (!clientes_idx) {
        printf(ERRO_MEMORIA_INSUFICIENTE);
        exit(1);
    }

    //recupera o registro de cada cliente e o insere no array clientes_idx
    //se o cliente foi excluido, apenas passa

    for (unsigned i = 0; i < qtd_registros_clientes; ++i) {
        Cliente c = recuperar_registro_cliente(i);

        if (strncmp(c.cpf, "*|", 2) == 0)
            continue; // registro excluído
        else
            clientes_idx[i].rrn = i;

        strcpy(clientes_idx[i].cpf, c.cpf);
        clientes_idx[i].cpf[TAM_CPF-1] = '\0';
    }

    qsort(clientes_idx, qtd_registros_clientes, sizeof(clientes_index), qsort_clientes_index);
}

/* (Re)faz o índice primário transacoes_index */
void criar_transacoes_index() {
    if (!transacoes_idx)
        transacoes_idx = malloc(MAX_REGISTROS * sizeof(transacoes_index));

    if (!transacoes_idx) {
        printf(ERRO_MEMORIA_INSUFICIENTE);
        exit(1);
    }   

    //faz o mesmo processo que pro criar_clientes_index
    //e atribui '\0' ao final da string

    for (unsigned i = 0; i < qtd_registros_transacoes; ++i) {
        Transacao t = recuperar_registro_transacao(i);

        if (strncmp(t.cpf_origem, "*|", 2) == 0)
            continue;
        else
            transacoes_idx[i].rrn = i;

        strcpy(transacoes_idx[i].cpf_origem, t.cpf_origem);
        transacoes_idx[i].cpf_origem[TAM_CPF-1] = '\0';
        strcpy(transacoes_idx[i].timestamp, t.timestamp);
        transacoes_idx[i].timestamp[TAM_TIMESTAMP-1] = '\0';
    }

    qsort(transacoes_idx, qtd_registros_transacoes, sizeof(transacoes_index), qsort_transacoes_index);
}

/* (Re)faz o índice secundário chaves_pix_index */
void criar_chaves_pix_index() {
    if (!chaves_pix_idx)
        chaves_pix_idx = malloc(MAX_REGISTROS * sizeof(chaves_pix_index));

    if (!chaves_pix_idx) {
        printf(ERRO_MEMORIA_INSUFICIENTE);
        exit(1);
    }

    //o mesmo dos dois indices anteriores, tambem com '\0' por conta do strcpy

    for (unsigned i = 0; i < qtd_registros_clientes; ++i) {
        Cliente c = recuperar_registro_cliente(i);

        if (strncmp(c.cpf, "*|", 2) == 0)
            continue;

        //percorre todas as chaves do cliente e as insere
        for(unsigned j = 0; j < 4; j++){
            if(c.chaves_pix[j][0]){
                strcpy(chaves_pix_idx[qtd_chaves_pix].pk_cliente, c.cpf);
                chaves_pix_idx[qtd_chaves_pix].pk_cliente[TAM_CPF-1] = '\0';
                strcpy(chaves_pix_idx[qtd_chaves_pix].chave_pix, c.chaves_pix[j] + 1);
                chaves_pix_idx[qtd_chaves_pix].chave_pix[TAM_MAX_CHAVE_PIX-1] = '\0';
                qtd_chaves_pix++;
            }
        }

    }

    qsort(chaves_pix_idx, qtd_chaves_pix, sizeof(chaves_pix_index), qsort_chaves_pix_index);
}

/* (Re)faz o índice secundário timestamp_cpf_origem_index */
void criar_timestamp_cpf_origem_index() {
    if (!timestamp_cpf_origem_idx)
        timestamp_cpf_origem_idx = malloc(MAX_REGISTROS * sizeof(timestamp_cpf_origem_index));

    if (!timestamp_cpf_origem_idx) {
        printf(ERRO_MEMORIA_INSUFICIENTE);
        exit(1);
    }

    //mesma logica dos tres anteriores

    for (unsigned i = 0; i < qtd_registros_transacoes; ++i) {
        Transacao t = recuperar_registro_transacao(i);

        if (strncmp(t.cpf_origem, "*|", 2) == 0)
            continue;

        //insere todos os recuperados que nao tiverem sido excluidos
        strcpy(timestamp_cpf_origem_idx[i].cpf_origem, t.cpf_origem);
        timestamp_cpf_origem_idx[i].cpf_origem[TAM_CPF-1] = '\0';
        strcpy(timestamp_cpf_origem_idx[i].timestamp, t.timestamp);
        timestamp_cpf_origem_idx[i].timestamp[TAM_TIMESTAMP-1] = '\0';
    }

    qsort(timestamp_cpf_origem_idx, qtd_registros_transacoes, sizeof(timestamp_cpf_origem_index), qsort_timestamp_cpf_origem_index);
}

/* Exibe um cliente dado seu RRN */
int exibir_cliente(int rrn) {
    if (rrn < 0)
        return 0;

    Cliente c = recuperar_registro_cliente(rrn);

    printf("%s, %s, %s, %s, %s, %.2lf, {", c.cpf, c.nome, c.nascimento, c.email, c.celular, c.saldo);

    int imprimiu = 0;
    for (int i = 0; i < 4; ++i) {
        if (c.chaves_pix[i][0] != '\0') {
            if (imprimiu)
                printf(", ");
            printf("%s", c.chaves_pix[i]);
            imprimiu = 1;
        }
    }
    printf("}\n");

    return 1;
}

/* Exibe uma transação dada seu RRN */
int exibir_transacao(int rrn) {
    if (rrn < 0)
        return 0;

    Transacao t = recuperar_registro_transacao(rrn);

    printf("%s, %s, %.2lf, %s\n", t.cpf_origem, t.cpf_destino, t.valor, t.timestamp);

    return 1;
}

/* Recupera do arquivo de clientes o registro com o RRN
 * informado e retorna os dados na struct Cliente */
Cliente recuperar_registro_cliente(int rrn) {
    Cliente c;
    char temp[TAM_REGISTRO_CLIENTE + 1], *p;
    strncpy(temp, ARQUIVO_CLIENTES + (rrn * TAM_REGISTRO_CLIENTE), TAM_REGISTRO_CLIENTE);
    temp[TAM_REGISTRO_CLIENTE] = '\0';

    //cria um ponteiro para o arquivo, na posicao rrn*TAM_REGISTRO_CLIENTE
    //percorre a string apontada por meio do strtok

    p = strtok(temp, ";");
    strcpy(c.cpf, p);
    p = strtok(NULL, ";");
    strcpy(c.nome, p);
    p = strtok(NULL, ";");
    strcpy(c.nascimento, p);
    p = strtok(NULL, ";");
    strcpy(c.email, p);
    p = strtok(NULL, ";");
    strcpy(c.celular, p);
    p = strtok(NULL, ";");
    c.saldo = atof(p);
    p = strtok(NULL, ";");

    for(int i = 0; i < 4; ++i)
        c.chaves_pix[i][0] = '\0';

    //percorre as chaves do registro obtido
    //e se for valida, as insere na struct

    if(p[0] != '#') {
        p = strtok(p, "&");

        for(int pos = 0; p; p = strtok(NULL, "&"), ++pos){
            strcpy(c.chaves_pix[pos], p);
            c.chaves_pix[pos][TAM_MAX_CHAVE_PIX-1] = '\0';
        }
            
    }

    return c;
}

/* Recupera do arquivo de transações o registro com o RRN
 * informado e retorna os dados na struct Transacao */
Transacao recuperar_registro_transacao(int rrn) {
    Transacao t;
    char temp[TAM_REGISTRO_TRANSACAO + 1], valor[TAM_SALDO];
    strncpy(temp, ARQUIVO_TRANSACOES + (rrn * TAM_REGISTRO_TRANSACAO), TAM_REGISTRO_TRANSACAO);
    temp[TAM_REGISTRO_TRANSACAO] = '\0';

    //atribui o endereco no arquivo a p, da mesma forma de recuperar cliente
    //percorre a string pelo strncpy, ja que todos os campos tem tam fixo

    strncpy(t.cpf_origem, temp, 11);
    t.cpf_origem[11] = '\0';
    strncpy(t.cpf_destino, temp + 11, 11);
    t.cpf_destino[11] = '\0';
    strncpy(valor, temp + 22, 13);
    valor[TAM_SALDO-1] = '\0';
    t.valor = atof(valor);
    strncpy(t.timestamp, temp + 35, 14);
    t.timestamp[14] = '\0';

    return t;
}

/* Funções principais */
void cadastrar_cliente(char *cpf, char *nome, char *nascimento, char *email, char *celular) {

    char cliente[TAM_REGISTRO_CLIENTE];


    //procura a pk do cliente no indice, se existir eh repetido
    for(int i = 0; i < qtd_registros_clientes; i++){
        if(strcmp(clientes_idx[i].cpf, cpf) == 0) { printf(ERRO_PK_REPETIDA, cpf); return; }
    }

    //monta a string que vai ser escrita
    sprintf(cliente, "%s;%s;%s;%s;%s;0000000000.00;;", cpf, nome, nascimento, email, celular);

    //completa a string com #
    while(strlen(cliente) < 256){
        strcat(cliente, "#");
    }

    
    cliente[TAM_REGISTRO_CLIENTE] = '\0';

    //copiando pro arquivo

    strcpy(ARQUIVO_CLIENTES + (qtd_registros_clientes * TAM_REGISTRO_CLIENTE), cliente);

    ARQUIVO_CLIENTES[TAM_ARQUIVO_CLIENTE] = '\0';

    //atualizando os indices na ultima posicao antes de aumentar a qtd de registros
    
    strcpy(clientes_idx[qtd_registros_clientes].cpf, cpf);

    clientes_idx[qtd_registros_clientes].cpf[TAM_CPF-1] = '\0';

    clientes_idx[qtd_registros_clientes].rrn = qtd_registros_clientes;

    qtd_registros_clientes++;

    //ordenando o indice dnv

    qsort(clientes_idx, qtd_registros_clientes, sizeof(clientes_index), qsort_clientes_index);

    printf(SUCESSO);

}

void remover_cliente(char *cpf) {

    int rrn = 0;

    if(qtd_registros_clientes == 0){
        printf(ERRO_ARQUIVO_VAZIO);
        return;
    }

    //bsearch pra achar o cliente a ser removido

    clientes_index *c;
    c = bsearch(cpf, clientes_idx, qtd_registros_clientes, sizeof(clientes_index), qsort_clientes_index);   


    if(c->cpf == NULL) {
        printf(ERRO_REGISTRO_NAO_ENCONTRADO);
        return;
    }

    //p na posicao certa pra remocao

    char *p = ARQUIVO_CLIENTES + c->rrn*TAM_REGISTRO_CLIENTE;

    //caracteres de remocao
    strncpy(p, "*|", 2);

    //ordenando indice
    qsort(clientes_idx, qtd_registros_clientes, sizeof(clientes_index), qsort_clientes_index);

    printf(SUCESSO);

}

void alterar_saldo(char *cpf, double valor) {

    if(valor == 0){
        printf(ERRO_VALOR_INVALIDO);
        return;
    }
    
    Cliente c;

    int rrn = -1;

    //busca pelo rrn do cliente com o cpf

    for(int i = 0; i < qtd_registros_clientes; i++){
        if(strcmp(clientes_idx[i].cpf, cpf) == 0){
            rrn = clientes_idx[i].rrn;
            break;
        } 
    }

    //se nao atribuir nenhum rrn, nao existe o cliente

    if(rrn == -1){
        printf(ERRO_REGISTRO_NAO_ENCONTRADO);
        return;
    }

    c = recuperar_registro_cliente(rrn);

    //recupera o cliente e compara o valor com o saldo

    if(valor < 0){

        char *valor1[TAM_SALDO], *saldo2[TAM_SALDO];

        sprintf(valor1, "%013.2lf", -valor);
        sprintf(saldo2, "%013.2lf", c.saldo);
        if(strcmp(saldo2, valor1) < 0){
            printf(ERRO_SALDO_NAO_SUFICIENTE);
            return;
        }

    }
    
    //escrevendo o saldo na formatacao certa
    char *saldo[TAM_SALDO];
    
    sprintf(saldo, "%013.2lf", c.saldo+valor);

    c.saldo = atof(saldo);

    //escrevendo no arquivo

    escrever_registro_cliente(c, rrn);
    
    printf(SUCESSO);

}

//alterar_saldoSEMOK eh o mesmo do alterar_saldo, mas pra ser usado na
//transacao nao poderia dar OKs repetidos, por isso a funcao nova

int alterar_saldoSEMOK(char *cpf, double valor, int rrn) {
    
    Cliente c;

    c = recuperar_registro_cliente(rrn);

    if(valor < 0){

        char *valor1[TAM_SALDO], *saldo2[TAM_SALDO];

        sprintf(valor1, "%013.2lf", -valor);
        sprintf(saldo2, "%013.2lf", c.saldo);
        if(strcmp(saldo2, valor1) < 0){
            return 1;
        }

    }
    
    char *saldo[TAM_SALDO];
    
    sprintf(saldo, "%013.2lf", c.saldo+valor);

    c.saldo = atof(saldo);

    escrever_registro_cliente(c, rrn);

    return 0;

}

void cadastrar_chave_pix(char *cpf, char tipo) {

    int rrn = -1;
    Cliente c;
    char chaveStruct[TAM_MAX_CHAVE_PIX]; 
    char chaveArquivo[TAM_MAX_CHAVE_PIX];

    //buscando pelo rrn do cliente que vai ter a chave

    for(int i = 0; i < qtd_registros_clientes; i++){
        if(strcmp(clientes_idx[i].cpf, cpf) == 0){
            rrn = clientes_idx[i].rrn;
            break;
        } 
    }

    if(rrn == -1){
        printf(ERRO_REGISTRO_NAO_ENCONTRADO);
        return;
    }

    c = recuperar_registro_cliente(rrn);

    int i;

    //verificacao se o cliente ja possui a chave
    //se tiver uma posicao livre, passa
    //se todas estiverem ocupadas, so pode ser uma chave repetida

    for (i = 0; i < 4; ++i) {
        if (c.chaves_pix[i][0] == '\0') {
            break;
        }
        if(c.chaves_pix[i][0] == tipo){
            printf(ERRO_CHAVE_PIX_DUPLICADA, tipo); 
            return;
        }
        if(i == 3){
            printf(ERRO_CHAVE_PIX_DUPLICADA, tipo); 
            return;
        }
    }

    //buffer pra armazenar a chave gerada
    char chaveBuffer[TAM_MAX_CHAVE_PIX];
    chaveBuffer[TAM_MAX_CHAVE_PIX] = '\0';

    //conjunto de ifs para comparar o tipo diretamente com o valor da letra na
    //tabela ASCII
    //deveria ser um switch, mas o switch estava bugando o valor quando chave = 'N'


    //tem duas chaves diferentes
    //a que vai ser escrita no arquivo, que tem o caractere na frente
    //e a da struct, sem o caractere
    if(tipo == 78){
        sprintf(chaveStruct, "%s", c.celular);
        sprintf(chaveArquivo, "N%s", c.celular);
    }
    else if(tipo == 65){
        new_uuid(chaveBuffer);
        sprintf(chaveStruct, "%s", chaveBuffer);
        sprintf(chaveArquivo, "A%s", chaveBuffer);
    }
    else if(tipo == 69){
        sprintf(chaveStruct, "%s", c.email);
        sprintf(chaveArquivo, "E%s", c.email);
    }
    else if(tipo == 67){
        sprintf(chaveStruct, "%s", c.cpf);
        sprintf(chaveArquivo, "C%s", c.cpf);
    }
    else{
        printf(ERRO_TIPO_PIX_INVALIDO, tipo);
        return;
    }
            
    strcpy(c.chaves_pix[i], chaveArquivo);

    c.chaves_pix[i][TAM_MAX_CHAVE_PIX-1] = '\0';

    //busca se outro usuario ja tem a mssma chave

    if(existe_chave(chaveStruct, cpf)){ printf(ERRO_CHAVE_PIX_REPETIDA, chaveStruct); return; }

    //atualizando indices
    strcpy(chaves_pix_idx[qtd_chaves_pix].chave_pix, chaveStruct);
    strcpy(chaves_pix_idx[qtd_chaves_pix].pk_cliente, cpf);

    chaves_pix_idx[qtd_chaves_pix].pk_cliente[TAM_CPF-1] = '\0';
    chaves_pix_idx[qtd_chaves_pix].chave_pix[TAM_MAX_CHAVE_PIX-1] = '\0';

    qtd_chaves_pix++;
    
    escrever_registro_cliente(c, rrn);

    qsort(chaves_pix_idx, qtd_chaves_pix, sizeof(chaves_pix_index), qsort_chaves_pix_index);

    printf(SUCESSO);

}

void escrever_registro_cliente(Cliente c, int rrn){

    char *p = ARQUIVO_CLIENTES + rrn*TAM_REGISTRO_CLIENTE;
    char *registro[TAM_REGISTRO_CLIENTE+1];

    sprintf(&registro, "%s;%s;%s;%s;%s;%013.2lf;", c.cpf, c.nome, c.nascimento, c.email, c.celular, c.saldo);

    //usando a mesma metodologia do exibir_cliente
    //mas para escrever em uma string
    int escreveu = 0;
    for (int i = 0; i < 4; ++i) {
        if (c.chaves_pix[i][0] != '\0') {
            if (escreveu)
                strcat(&registro, "&");
            strcat(&registro, c.chaves_pix[i]);
            escreveu = 1;
        }
    }

    strcat(&registro, ";");

    while(strlen(registro) < 256){
        strcat(registro, "#");
    }

    strncpy(p, registro, 256);

}

int existe_chave(char *chave_pix, char *cpf) {

    //bsearch para buscar a chave de um cliente;

    chaves_pix_index *c;
    c = bsearch(chave_pix, chaves_pix_idx, qtd_chaves_pix, sizeof(chaves_pix_index), qsort_chaves_pix_index);

    if(c->chave_pix != NULL) return 1;
    else return 0;
    
}

void transferir(char *chave_pix_origem, char *chave_pix_destino, double valor) {

    char cpfOrigem[TAM_CPF], cpfDestino[TAM_CPF];
    
    if(valor <= 0){ printf(ERRO_VALOR_INVALIDO); return; }

    //criando struct e buscando no chaves_pix_idx o cliente que possui a chave
    //para o cliente de origem e de destino

    chaves_pix_index *clienteOrigem = malloc(sizeof(chaves_pix_index)); 
    strcpy(clienteOrigem->chave_pix, chave_pix_origem);
    clienteOrigem = bsearch(clienteOrigem, chaves_pix_idx, qtd_chaves_pix, sizeof(chaves_pix_index), qsort_chaves_pix_index);

    chaves_pix_index *clienteDestino = malloc(sizeof(chaves_pix_index)); 
    strcpy(clienteDestino->chave_pix, chave_pix_destino);
    clienteDestino = bsearch(clienteDestino, chaves_pix_idx, qtd_chaves_pix, sizeof(chaves_pix_index), qsort_chaves_pix_index);


    //se nao achar os clientes, chave errada
    if(clienteDestino == NULL || clienteOrigem == NULL){
        printf(ERRO_CHAVE_PIX_INVALIDA);
        return;
    }else {
        strcpy(cpfDestino, clienteDestino->pk_cliente);
        strcpy(cpfOrigem, clienteOrigem->pk_cliente);
        cpfOrigem[TAM_CPF - 1] = '\0';
        cpfDestino[TAM_CPF -1] = '\0';
    }

    //se os clientes forem iguais, invalido

    if(strcmp(cpfOrigem, cpfDestino) == 0){
        printf(ERRO_CHAVE_PIX_INVALIDA);
        return;
    }
    
    Transacao t;

    char timestamp[TAM_TIMESTAMP];
    
    current_timestamp(timestamp);

    strcpy(t.cpf_destino, cpfDestino);
    t.cpf_destino[TAM_CPF-1] = '\0';
    strcpy(t.cpf_origem, cpfOrigem);
    t.cpf_origem[TAM_CPF-1] = '\0';
    strcpy(t.timestamp, timestamp);
    t.timestamp[TAM_TIMESTAMP-1] = '\0';
    t.valor = valor;

    //finalmente, buscando rrns dos clientes finais no index de clientes

    clientes_index *c = malloc(sizeof(chaves_pix_index)); 
    strcpy(c->cpf, clienteOrigem->pk_cliente);
    c->cpf[TAM_CPF-1] = '\0';
    c = bsearch(c, clientes_idx, qtd_registros_clientes, sizeof(clientes_index), qsort_clientes_index);

    clientes_index *d = malloc(sizeof(chaves_pix_index)); 
    strcpy(d->cpf, clienteDestino->pk_cliente);
    d->cpf[TAM_CPF-1] = '\0';
    d = bsearch(d, clientes_idx, qtd_registros_clientes, sizeof(clientes_index), qsort_clientes_index);

    //alterando os saldos com os rrns obtidos
    if(alterar_saldoSEMOK(cpfOrigem, -valor, c->rrn) == 1) { printf(ERRO_SALDO_NAO_SUFICIENTE); return; };
    if(alterar_saldoSEMOK(cpfDestino, valor, d->rrn) == 1) { printf(ERRO_SALDO_NAO_SUFICIENTE); return; };

    escrever_arquivo_transacoes(t, qtd_registros_transacoes);

    //atualizando os indices

    strcpy(transacoes_idx[qtd_registros_transacoes].cpf_origem, cpfOrigem);
    transacoes_idx[qtd_registros_transacoes].cpf_origem[TAM_CPF-1] = '\0';
    strcpy(transacoes_idx[qtd_registros_transacoes].timestamp, timestamp);
    transacoes_idx[qtd_registros_transacoes].timestamp[TAM_TIMESTAMP-1] = '\0';
    transacoes_idx[qtd_registros_transacoes].rrn = qtd_registros_transacoes;

    strcpy(timestamp_cpf_origem_idx[qtd_registros_transacoes].cpf_origem, cpfOrigem);
    timestamp_cpf_origem_idx[qtd_registros_transacoes].cpf_origem[TAM_CPF-1] = '\0';
    strcpy(timestamp_cpf_origem_idx[qtd_registros_transacoes].timestamp, timestamp);
    timestamp_cpf_origem_idx[qtd_registros_transacoes].timestamp[TAM_TIMESTAMP-1] = '\0';
    
    qtd_registros_transacoes++;

    qsort(transacoes_idx, qtd_registros_transacoes, sizeof(transacoes_index), qsort_transacoes_index);

    qsort(timestamp_cpf_origem_idx, qtd_registros_transacoes, sizeof(timestamp_cpf_origem_index), qsort_timestamp_cpf_origem_index);

    printf(SUCESSO);

}

void escrever_arquivo_transacoes(Transacao t, int rrn){

    //escreve no arquivo de transacoes da mesma forma que no de clientes
    //mas com as especificidades de uma transacao

    char *p = ARQUIVO_TRANSACOES + rrn*TAM_REGISTRO_TRANSACAO;

    char *registro[TAM_REGISTRO_TRANSACAO+1];

    sprintf(registro, "%s%s%013.2lf%s", t.cpf_origem, t.cpf_destino, t.timestamp, t.valor);

    strncpy(p, registro, TAM_REGISTRO_TRANSACAO);
    
    ARQUIVO_TRANSACOES[TAM_ARQUIVO_TRANSACAO] = '\0';

}

/* Busca */
void buscar_cliente_cpf(char *cpf) {

    //busca o cliente no index de clientes por cpf, busca simples

    clientes_index *c;
    c = bsearch(cpf, clientes_idx, qtd_registros_clientes, sizeof(clientes_index), qsort_clientes_index);   

    if(c->cpf != NULL){
        exibir_cliente(c->rrn);
    }else printf(ERRO_REGISTRO_NAO_ENCONTRADO);
    
}

void buscar_cliente_chave_pix(char *chave_pix) {

    //busca como a de cliente, mas de chave pix no index respectivo

    chaves_pix_index *c;
    c = bsearch(chave_pix, chaves_pix_idx, qtd_chaves_pix, sizeof(chaves_pix_index), qsort_chaves_pix_index);

    //se encontrar ,exibe
    if(c->chave_pix != NULL){
        buscar_cliente_cpf(c->pk_cliente);
    }else printf(ERRO_REGISTRO_NAO_ENCONTRADO);

}

void buscar_transacao_cpf_origem_timestamp(char *cpf, char *timestamp) {

    //constroi uma struct de transacao com os dados e busca no index de transacoes

    transacoes_index *busca = malloc(sizeof(transacoes_index));
    strcpy(busca->cpf_origem, cpf);
    busca->cpf_origem[TAM_CPF-1] = '\0';
    strcpy(busca->timestamp, timestamp);
    busca->timestamp[TAM_TIMESTAMP-1] = '\0';

    transacoes_index *t = bsearch(busca, transacoes_idx, qtd_registros_transacoes, sizeof(transacoes_index), qsort_transacoes_index);

    //se encontrar, exibe
    if(t != NULL){
        exibir_transacao(t->rrn);
    }else {
        printf(ERRO_REGISTRO_NAO_ENCONTRADO);
    }

    free(busca);
    return;
    
}

/* Listagem */
void listar_clientes_cpf() {

    //listagem de clientes por iteracao, ja que e apenas para imprimir tudo

    if(qtd_registros_clientes == 0) { printf(AVISO_NENHUM_REGISTRO_ENCONTRADO); return; }

    for(int i = 0; i < qtd_registros_clientes; i++){
        exibir_cliente(clientes_idx[i].rrn);
    }


}

void listar_transacoes_periodo(char *data_inicio, char *data_fim) {

    if(qtd_registros_transacoes == 0) { printf(AVISO_NENHUM_REGISTRO_ENCONTRADO); return; }

    //busca o indice do teto da data inicio e piso da data fim no indice de
    //timestamp_cpf
    int comecoIntervalo = binarySearchCeiling(data_inicio);
    int fimIntervalo = binarySearchFloor(data_fim);

    if(fimIntervalo < comecoIntervalo){ printf(AVISO_NENHUM_REGISTRO_ENCONTRADO); return; }

    //percore o timestamp_cpf_origem_idx em todas as posicoes entre o indice
    //inicial e final recuperados 
    for(comecoIntervalo; comecoIntervalo <= fimIntervalo; comecoIntervalo++){

        transacoes_index *t = malloc(sizeof(transacoes_index));
        strcpy(t->cpf_origem, timestamp_cpf_origem_idx[comecoIntervalo].cpf_origem);
        t->cpf_origem[TAM_CPF] = '\0';
        strcpy(t->timestamp, timestamp_cpf_origem_idx[comecoIntervalo].timestamp);
        t->timestamp[TAM_TIMESTAMP] = '\0';
        
        //recupera o rrn do timestamp_cpf_origem_idx, e o usa pra buscar
        //no transacoes_idx
        t = bsearch(t, transacoes_idx, qtd_registros_transacoes, sizeof(transacoes_index), qsort_transacoes_index);
        exibir_transacao(t->rrn);
    
    }

}

int binarySearchFloor(char *data_fim){

    int direita = qtd_registros_transacoes-1;
    int esquerda = 0;

    int i = 0, piso = -1;
    while (esquerda <= direita) {

        int m = esquerda + (direita - esquerda) / 2;

        //se for maior ou igual, atribui m ao piso
        //dessa forma, sempre teremos automaticamente o piso, ja que m
        //representa o valor diretamente maior
  
        if (strcmp(data_fim, timestamp_cpf_origem_idx[m].timestamp) >= 0) {
            piso = m;
            esquerda = m + 1;
        }
        else
            direita = m - 1;
    }
  
    return piso;
}


int binarySearchCeiling(char *data_inicio){

    int direita = qtd_registros_transacoes-1;
    int esquerda = 0;

    int i = 0, teto;

    while (esquerda <= direita) {

        int m = esquerda + (direita - esquerda) / 2;

        //se for menor ou igual, atribui m ao teto
        //dessa forma, sempre teremos automaticamente o teto, ja que m
        //representa o valor diretamente menor
  
        if (strcmp(data_inicio, timestamp_cpf_origem_idx[m].timestamp) <= 0) {
            teto = m;
            direita = m - 1;
        }
        else
            esquerda = m + 1;
    }

    return teto;

}

void listar_transacoes_cpf_origem_periodo(char *cpf, char *data_inicio, char *data_fim) {
    
    if(qtd_registros_transacoes == 0) { printf(AVISO_NENHUM_REGISTRO_ENCONTRADO); return; }

    //como na listagem anterior, busca os indices do intervalo no transacoes idx
    int comecoIntervalo = binarySearchCeiling2(data_inicio, cpf);
    int fimIntervalo = binarySearchFloor2(data_fim, cpf);

    if(fimIntervalo < comecoIntervalo){ printf(AVISO_NENHUM_REGISTRO_ENCONTRADO); return; }

    //percorre o transacoes idx exibindo caso o cpf seja igual
    for(comecoIntervalo; comecoIntervalo <= fimIntervalo; comecoIntervalo++){

        if(strcmp(transacoes_idx[comecoIntervalo].cpf_origem, cpf) == 0)
        exibir_transacao(transacoes_idx[comecoIntervalo].rrn);

    }

}

int binarySearchFloor2(char *data_inicio, char *cpf){

    int direita = qtd_registros_transacoes-1;
    transacoes_index *u = malloc(sizeof(transacoes_index));

    //mesma busca do binarySearchFloor, mas usando uma struct de transacoes_index
    //para pesquisar
    //e usando as comparacores do qsort_transacoes_index
    //com a mesma logica

    strcpy(u->cpf_origem, cpf);
    u->cpf_origem[TAM_CPF-1] = '\0';
    strcpy(u->timestamp, data_inicio);
    u->timestamp[TAM_TIMESTAMP-1] = '\0';

    int esquerda = 0;

    int i = 0, piso = -1;
    while (esquerda <= direita) {

        int m = esquerda + (direita - esquerda) / 2;

        if (qsort_transacoes_index(u, &transacoes_idx[m]) >= 0) {
            piso = m;
            esquerda = m + 1;
        }
        else
            direita = m - 1;

    }
  
    return piso;
}

int binarySearchCeiling2(char *data_fim, char *cpf){

    int direita = qtd_registros_transacoes-1;

    transacoes_index *u = malloc(sizeof(transacoes_index));

    //mesma busca do binarySearchCeiling, mas usando uma struct de transacoes_index
    //para pesquisar
    //e usando as comparacores do qsort_transacoes_index
    //com a mesma logica

    strcpy(u->cpf_origem, cpf);
    u->cpf_origem[TAM_CPF-1] = '\0';
    strcpy(u->timestamp, data_fim);
    u->timestamp[TAM_TIMESTAMP-1] = '\0';

    int esquerda = 0;

    int i = 0, teto;
    while (esquerda <= direita) {

        int m = esquerda + (direita - esquerda) / 2;

        if (qsort_transacoes_index(u, &transacoes_idx[m]) <= 0) {
            teto = m;
            direita = m - 1;
        }
        else
            esquerda = m + 1;

    }
  
    return teto;
}

/* Liberar espaço */
void liberar_espaco() {
    /* <<< COMPLETE AQUI A IMPLEMENTAÇÃO >>> */
    printf(ERRO_NAO_IMPLEMENTADO, "liberar_espaco");
}

/* Imprimir arquivos de dados */
void imprimir_arquivo_clientes() {
    if (qtd_registros_clientes == 0)
        printf(ERRO_ARQUIVO_VAZIO);
    else
        printf("%s\n", ARQUIVO_CLIENTES);
}

void imprimir_arquivo_transacoes() {
    if (qtd_registros_transacoes == 0)
        printf(ERRO_ARQUIVO_VAZIO);
    else
        printf("%s\n", ARQUIVO_TRANSACOES);
}

/* Imprimir índices secundários */
void imprimir_chaves_pix_index() {
    if (qtd_chaves_pix == 0)
        printf(ERRO_ARQUIVO_VAZIO);

    for (unsigned i = 0; i < qtd_chaves_pix; ++i)
        printf("%s, %s\n", chaves_pix_idx[i].chave_pix, chaves_pix_idx[i].pk_cliente);
}

void imprimir_timestamp_cpf_origem_index() {
    if (qtd_registros_transacoes == 0)
        printf(ERRO_ARQUIVO_VAZIO);

    for (unsigned i = 0; i < qtd_registros_transacoes; ++i)
        printf("%s, %s\n", timestamp_cpf_origem_idx[i].timestamp, timestamp_cpf_origem_idx[i].cpf_origem);
}

/* Liberar memória e encerrar programa */
void liberar_memoria() {
    free(clientes_idx);
    free(timestamp_cpf_origem_idx);
    free(transacoes_idx);
    free(chaves_pix_idx);
}

/* Função de comparação entre clientes_index */
int qsort_clientes_index(const void *a, const void *b) {
    return strcmp(( (clientes_index *)a ), ( (clientes_index*)b ));
}



/* Função de comparação entre transacoes_index */
int qsort_transacoes_index(const void *a, const void *b) {

    //se cpf == cpf, toma a decisao pelo timestamp
    //senao toma a decisao pelo cpf    
    int compare = strcmp(((transacoes_index *)a )-> cpf_origem, ( (transacoes_index*)b )-> cpf_origem);

    if(compare == 0);
        return strcmp(((transacoes_index *)a )-> timestamp, ( (transacoes_index*)b )-> timestamp);

    return compare;

}       

/* Função de comparação entre chaves_pix_index */
int qsort_chaves_pix_index(const void *a, const void *b) {
    return strcmp(((chaves_pix_index *)a ), ( (chaves_pix_index*)b ));
}

/* Função de comparação entre timestamp_cpf_origem_index */
int qsort_timestamp_cpf_origem_index(const void *a, const void *b) {

    //se timestamp == timestamp, toma a decisao pelo cpf
    //senao toma a decisao pelo timestamp
    
    int compare = strcmp(((timestamp_cpf_origem_index *)a )-> timestamp, ( (timestamp_cpf_origem_index*)b )-> timestamp);
    
    if(compare == 0)
        return strcmp(((timestamp_cpf_origem_index *)a )-> cpf_origem, ( (timestamp_cpf_origem_index*)b )-> cpf_origem);
        
    return compare;
    
}

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <omp.h>

using namespace std;

// ============================================================================
// DEFINICIONES HISTÓRICAS DE LA MÁQUINA ENIGMA (Heer / Luftwaffe / Kriegsmarine M3)
// ============================================================================

struct RotorDef {
    const char* name;
    const char* wiring;
    char notch;
};

const RotorDef HISTORICAL_ROTORS[5] = {
    {"I",   "EKMFLGDQVZNTOWYHXUSPAIBRCJ", 'Q'},
    {"II",  "AJDKSIRUXBLHWTMCQGZNPYFVOE", 'E'},
    {"III", "BDFHJLCPRTXVZNYEIWGAKMUSQO", 'V'},
    {"IV",  "ESOVPZJAYQUIRHXLNFTGKDCMWB", 'J'},
    {"V",   "VZBRGITYUPSDNHLXAWMJQOFECK", 'Z'}
};

const char* REFLECTOR_B = "YRUHQSLDPXNGOKMIEBFZCWVJAT";
const char* REFLECTOR_C = "FVPJIAOYEDRZXWGCTKUQSBNMHL";

// Estructura de Rotor optimizada para tiempo de ejecución (LUT directa)
struct Rotor {
    uint8_t forward[26];
    uint8_t backward[26];
    uint8_t notch;
    uint8_t pos;   // Posición actual de la ventana (0..25)
    uint8_t ring;  // Ringstellung (0..25)

    void init(const string& wiring, char notch_char, int init_ring = 0, int init_pos = 0) {
        notch = notch_char - 'A';
        ring = init_ring;
        pos = init_pos;
        for (int i = 0; i < 26; ++i) {
            uint8_t out = wiring[i] - 'A';
            forward[i] = out;
            backward[out] = i;
        }
    }

    inline bool at_notch() const {
        return pos == notch;
    }

    inline void step() {
        pos = (pos == 25) ? 0 : pos + 1;
    }

    inline uint8_t enc_forward(uint8_t c) const {
        int shift = (pos >= ring) ? (pos - ring) : (pos - ring + 26);
        int in_pin = c + shift;
        if (in_pin >= 26) in_pin -= 26;
        int out_pin = forward[in_pin];
        int res = out_pin - shift;
        if (res < 0) res += 26;
        return (uint8_t)res;
    }

    inline uint8_t enc_backward(uint8_t c) const {
        int shift = (pos >= ring) ? (pos - ring) : (pos - ring + 26);
        int in_pin = c + shift;
        if (in_pin >= 26) in_pin -= 26;
        int out_pin = backward[in_pin];
        int res = out_pin - shift;
        if (res < 0) res += 26;
        return (uint8_t)res;
    }
};

// ============================================================================
// MOTOR PRINCIPAL DE ENIGMA
// ============================================================================

class Enigma {
public:
    Rotor left, mid, right;
    uint8_t reflector[26];
    uint8_t plugboard[26];

    int rotor_idx[3];
    int rings[3];
    int start_pos[3];
    string plug_str;
    char ref_choice;

    Enigma() {
        ref_choice = 'B';
        set_reflector('B');
        reset_plugboard();
    }

    void reset_plugboard() {
        for (int i = 0; i < 26; ++i) plugboard[i] = i;
    }

    void set_reflector(char c) {
        ref_choice = toupper(c);
        const char* ref_wiring = (ref_choice == 'C') ? REFLECTOR_C : REFLECTOR_B;
        for (int i = 0; i < 26; ++i) {
            reflector[i] = ref_wiring[i] - 'A';
        }
    }

    void set_plugboard(const string& pairs) {
        reset_plugboard();
        plug_str = pairs;
        stringstream ss(pairs);
        string p;
        while (ss >> p) {
            if (p.size() == 2 && isalpha(p[0]) && isalpha(p[1])) {
                int a = toupper(p[0]) - 'A';
                int b = toupper(p[1]) - 'A';
                plugboard[a] = b;
                plugboard[b] = a;
            }
        }
    }

    void set_raw_plugboard(const uint8_t* plugs) {
        memcpy(plugboard, plugs, 26);
    }

    void configure_indices(int idx_l, int idx_m, int idx_r,
                           int ring_l, int ring_m, int ring_r,
                           int pos_l, int pos_m, int pos_r,
                           char reflector_type = 'B',
                           const uint8_t* raw_plugs = nullptr) {
        rotor_idx[0] = idx_l; rotor_idx[1] = idx_m; rotor_idx[2] = idx_r;
        rings[0] = ring_l; rings[1] = ring_m; rings[2] = ring_r;
        start_pos[0] = pos_l; start_pos[1] = pos_m; start_pos[2] = pos_r;

        left.init(HISTORICAL_ROTORS[idx_l].wiring, HISTORICAL_ROTORS[idx_l].notch, ring_l, pos_l);
        mid.init(HISTORICAL_ROTORS[idx_m].wiring, HISTORICAL_ROTORS[idx_m].notch, ring_m, pos_m);
        right.init(HISTORICAL_ROTORS[idx_r].wiring, HISTORICAL_ROTORS[idx_r].notch, ring_r, pos_r);

        set_reflector(reflector_type);
        if (raw_plugs) set_raw_plugboard(raw_plugs);
        else reset_plugboard();
    }

    inline uint8_t process_letter(uint8_t c) {
        bool mid_at_notch = mid.at_notch();
        bool right_at_notch = right.at_notch();

        if (mid_at_notch) {
            mid.step();
            left.step();
        } else if (right_at_notch) {
            mid.step();
        }
        right.step();

        c = plugboard[c];
        c = right.enc_forward(c);
        c = mid.enc_forward(c);
        c = left.enc_forward(c);
        c = reflector[c];
        c = left.enc_backward(c);
        c = mid.enc_backward(c);
        c = right.enc_backward(c);
        c = plugboard[c];
        return c;
    }

    string process_text(const string& input, bool preserve_format = true) {
        string output;
        output.reserve(input.size() + input.size() / 5);

        int letter_count = 0;
        for (char ch : input) {
            if (isalpha(ch)) {
                bool is_lower = islower(ch);
                uint8_t in_c = (uint8_t)(toupper(ch) - 'A');
                uint8_t out_c = process_letter(in_c);
                char res_char = (char)(out_c + 'A');

                if (preserve_format) {
                    output += is_lower ? (char)tolower(res_char) : res_char;
                } else {
                    if (letter_count > 0 && letter_count % 5 == 0) output += ' ';
                    output += res_char;
                    letter_count++;
                }
            } else if (preserve_format) {
                output += ch;
            }
        }
        return output;
    }
};

// ============================================================================
// PARSEO Y VALIDACIÓN ROBUSTA DE ENTRADAS
// ============================================================================

vector<string> split_tokens(const string& s) {
    vector<string> tokens;
    string cur = "";
    for (char c : s) {
        if (c == ',' || c == '-' || c == '/' || isspace(c)) {
            if (!cur.empty()) { tokens.push_back(cur); cur = ""; }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

bool parse_single_rotor(string token, int& out_idx) {
    for (char& c : token) c = toupper(c);
    if (token == "I" || token == "1") { out_idx = 0; return true; }
    if (token == "II" || token == "2") { out_idx = 1; return true; }
    if (token == "III" || token == "3") { out_idx = 2; return true; }
    if (token == "IV" || token == "4") { out_idx = 3; return true; }
    if (token == "V" || token == "5") { out_idx = 4; return true; }
    return false;
}

bool validate_rotors_input(string input, int& r1, int& r2, int& r3, string& err_msg) {
    // Si está vacío, por defecto I,II,III
    if (input.empty()) {
        r1 = 0; r2 = 1; r3 = 2;
        return true;
    }

    auto tokens = split_tokens(input);

    // Caso especial: si es de 3 dígitos directos como "123"
    if (tokens.size() == 1 && tokens[0].size() == 3 && isdigit(tokens[0][0])) {
        string s = tokens[0];
        tokens = {string(1, s[0]), string(1, s[1]), string(1, s[2])};
    }

    if (tokens.size() != 3) {
        err_msg = "Debes ingresar exactamente 3 rotores (ej: I,II,III o 1,2,3 o I-II-III).";
        return false;
    }

    if (!parse_single_rotor(tokens[0], r1)) {
        err_msg = "Rotor 1 invalido ('" + tokens[0] + "'). Debe ser del I al V (1 a 5).";
        return false;
    }
    if (!parse_single_rotor(tokens[1], r2)) {
        err_msg = "Rotor 2 invalido ('" + tokens[1] + "'). Debe ser del I al V (1 a 5).";
        return false;
    }
    if (!parse_single_rotor(tokens[2], r3)) {
        err_msg = "Rotor 3 invalido ('" + tokens[2] + "'). Debe ser del I al V (1 a 5).";
        return false;
    }

    return true;
}

bool validate_positions_input(string input, int& p1, int& p2, int& p3, string& err_msg) {
    if (input.empty()) {
        p1 = 0; p2 = 0; p3 = 0;
        return true;
    }

    auto tokens = split_tokens(input);
    if (tokens.size() == 1 && tokens[0].size() == 3) {
        string s = tokens[0];
        tokens = {string(1, s[0]), string(1, s[1]), string(1, s[2])};
    }

    if (tokens.size() != 3) {
        err_msg = "Debes ingresar 3 letras para la posicion inicial (ej: AAA o A-A-A).";
        return false;
    }

    for (int i = 0; i < 3; ++i) {
        if (tokens[i].size() != 1 || !isalpha(tokens[i][0])) {
            err_msg = "Posicion '" + tokens[i] + "' invalida. Debe ser una letra (A-Z).";
            return false;
        }
    }

    p1 = toupper(tokens[0][0]) - 'A';
    p2 = toupper(tokens[1][0]) - 'A';
    p3 = toupper(tokens[2][0]) - 'A';
    return true;
}

bool validate_rings_input(string input, int& g1, int& g2, int& g3, string& err_msg) {
    if (input.empty()) {
        g1 = 0; g2 = 0; g3 = 0;
        return true;
    }

    auto tokens = split_tokens(input);
    if (tokens.size() == 1 && tokens[0].size() == 3 && isalpha(tokens[0][0])) {
        string s = tokens[0];
        tokens = {string(1, s[0]), string(1, s[1]), string(1, s[2])};
    }

    if (tokens.size() != 3) {
        err_msg = "Debes ingresar 3 valores para los anillos (ej: 01,01,01 o AAA o 1-1-1).";
        return false;
    }

    auto parse_val = [](const string& tok, int& out) -> bool {
        if (isdigit(tok[0])) {
            int v = stoi(tok);
            if (v >= 1 && v <= 26) { out = v - 1; return true; }
            return false;
        } else if (isalpha(tok[0])) {
            out = toupper(tok[0]) - 'A';
            return true;
        }
        return false;
    };

    if (!parse_val(tokens[0], g1) || !parse_val(tokens[1], g2) || !parse_val(tokens[2], g3)) {
        err_msg = "Valores de anillo invalidos. Use numeros del 1 al 26 o letras de la A a la Z.";
        return false;
    }
    return true;
}

bool validate_reflector_input(string input, char& ref, string& err_msg) {
    if (input.empty()) {
        ref = 'B';
        return true;
    }
    string s = "";
    for (char c : input) if (isalpha(c)) s += (char)toupper(c);
    if (s == "B" || s == "UKWB") { ref = 'B'; return true; }
    if (s == "C" || s == "UKWC") { ref = 'C'; return true; }
    err_msg = "Reflector invalido ('" + input + "'). Solo existen reflectores B o C.";
    return false;
}

bool validate_plugboard_input(string input, string& clean_plugs, string& err_msg) {
    clean_plugs = "";
    if (input.empty()) return true;

    stringstream ss(input);
    string pair;
    bool used[26] = {false};
    int count = 0;

    while (ss >> pair) {
        if (pair.size() != 2 || !isalpha(pair[0]) || !isalpha(pair[1])) {
            err_msg = "Par de clavijas invalido: '" + pair + "'. Cada par debe ser de 2 letras (ej: AV).";
            return false;
        }
        char a = toupper(pair[0]);
        char b = toupper(pair[1]);
        if (a == b) {
            err_msg = "No puedes conectar una letra consigo misma ('" + pair + "').";
            return false;
        }
        if (used[a - 'A']) {
            err_msg = "La letra '" + string(1, a) + "' ya esta conectada a otra clavija.";
            return false;
        }
        if (used[b - 'A']) {
            err_msg = "La letra '" + string(1, b) + "' ya esta conectada a otra clavija.";
            return false;
        }
        used[a - 'A'] = true;
        used[b - 'A'] = true;

        if (!clean_plugs.empty()) clean_plugs += " ";
        clean_plugs += a; clean_plugs += b;
        count++;
    }

    if (count > 10) {
        err_msg = "La maquina Enigma solo admite un maximo de 10 cables en el clavijero.";
        return false;
    }
    return true;
}

// ============================================================================
// MODELOS ESTADÍSTICOS DE IDIOMAS (ESPAÑOL, ALEMÁN, INGLÉS)
// ============================================================================

enum Language { LANG_ES, LANG_DE, LANG_EN };

struct LanguageModel {
    double bigram_weights[26][26];

    void init(Language lang) {
        const double mono_es[26] = {
            -2.07, -4.25, -3.06, -2.83, -1.98, -4.97, -4.59, -4.96,
            -2.77, -5.42, -9.21, -3.00, -3.45, -2.70, -2.44, -3.68,
            -4.73, -2.67, -2.52, -3.07, -3.23, -4.71, -9.21, -6.11,
            -4.71, -5.25
        };
        const double mono_de[26] = {
            -2.73, -3.96, -3.48, -2.97, -1.80, -4.09, -3.50, -3.08,
            -2.58, -5.91, -4.41, -3.36, -3.67, -2.32, -3.68, -4.84,
            -8.51, -2.65, -2.62, -2.78, -3.13, -5.00, -3.96, -8.11,
            -7.82, -4.48
        };
        const double mono_en[26] = {
            -2.50, -4.20, -3.57, -2.93, -2.06, -3.79, -3.91, -2.79,
            -2.66, -7.26, -5.11, -3.21, -3.73, -2.70, -2.59, -4.00,
            -7.01, -2.81, -2.76, -2.40, -3.57, -4.62, -3.86, -7.26,
            -3.91, -7.26
        };

        const double* chosen_mono = (lang == LANG_ES) ? mono_es : (lang == LANG_DE ? mono_de : mono_en);

        for (int i = 0; i < 26; ++i) {
            for (int j = 0; j < 26; ++j) {
                bigram_weights[i][j] = chosen_mono[i] + chosen_mono[j];
            }
        }

        if (lang == LANG_ES) {
            const char* top_es[] = {
                "DE", "ES", "EN", "EL", "LA", "OS", "ON", "ER", "AS", "AR",
                "RA", "RE", "TE", "AL", "OR", "CO", "UE", "NT", "TA", "CI",
                "IO", "DO", "AN", "QU", "AD", "SE", "TR", "PO", "TO", "IN",
                "ST", "MA", "RO", "AC", "UN", "CA", "SO", "PA", "ME", "DI",
                "PR", "OR", "LE", "DA", "LL", "CH", "RR", "PE", "LO", "SU"
            };
            for (const char* b : top_es) bigram_weights[b[0]-'A'][b[1]-'A'] += 3.0;
        } else if (lang == LANG_DE) {
            const char* top_de[] = {
                "ER", "EN", "CH", "DE", "EI", "TE", "IN", "ND", "IE", "GE",
                "ES", "NE", "UN", "ST", "RE", "HE", "AN", "BE", "SE", "NG",
                "DI", "IC", "SC", "AU", "DA", "IT", "LE", "ZU", "OR", "SI"
            };
            for (const char* b : top_de) bigram_weights[b[0]-'A'][b[1]-'A'] += 3.0;
        } else {
            const char* top_en[] = {
                "TH", "HE", "IN", "ER", "AN", "RE", "ED", "ON", "ES", "ST",
                "EN", "AT", "TO", "NT", "HA", "ND", "OU", "EA", "NG", "AS"
            };
            for (const char* b : top_en) bigram_weights[b[0]-'A'][b[1]-'A'] += 3.0;
        }
    }

    double score(const vector<uint8_t>& text) const {
        if (text.size() < 2) return -1e9;
        double s = 0.0;
        for (size_t i = 0; i < text.size() - 1; ++i) {
            s += bigram_weights[text[i]][text[i+1]];
        }
        return s;
    }
};

double calc_ioc(const vector<uint8_t>& text) {
    if (text.size() < 2) return 0.0;
    int counts[26] = {0};
    for (uint8_t c : text) counts[c]++;
    long long sum = 0;
    for (int i = 0; i < 26; ++i) sum += (long long)counts[i] * (counts[i] - 1);
    return (double)sum / ((double)text.size() * (text.size() - 1));
}

// ============================================================================
// DESCIFRADOR MODERNO 100% A CIEGAS (Ciphertext-Only)
// ============================================================================

struct ModernCrackResult {
    string rotors;
    string positions;
    string plugboard;
    string plaintext;
    double ioc;
    double language_score;
};

ModernCrackResult run_modern_blind_crack(const string& ciphertext, Language lang = LANG_ES, char reflector_type = 'B') {
    cout << "\n=======================================================\n";
    cout << "      DESCIFRADOR MODERNO 100% A CIEGAS (CIPHERTEXT-ONLY)\n";
    cout << "=======================================================\n";
    cout << "Idioma objetivo : " << (lang == LANG_ES ? "Espanol" : (lang == LANG_DE ? "Aleman" : "Ingles")) << "\n";
    cout << "Texto cifrado   : " << ciphertext.substr(0, 50) << (ciphertext.size() > 50 ? "..." : "") << "\n";

    vector<uint8_t> clean_cipher;
    for (char c : ciphertext) if (isalpha(c)) clean_cipher.push_back(toupper(c) - 'A');

    cout << "Longitud util   : " << clean_cipher.size() << " letras\n";
    cout << "IoC del cifrado : " << fixed << setprecision(5) << calc_ioc(clean_cipher) << " (Ruido ~0.038)\n\n";

    if (clean_cipher.size() < 70) {
        cout << "[!] AVISO IMPORTANTE (Distancia de Unicidad de Shannon):\n";
        cout << "    El texto tiene " << clean_cipher.size() << " letras. Para descifrado a ciegas\n";
        cout << "    (sin conocer clave ni palabras) se recomiendan al menos 80-100 letras.\n";
        cout << "    Para mensajes cortos como este, la tecnica mas precisa es el\n";
        cout << "    Ataque por CRIB (Opcion 3), indicando una palabra sospechada.\n\n";
    }

    LanguageModel lm;
    lm.init(lang);

    auto t0 = chrono::high_resolution_clock::now();

    // 1. Generamos TODAS las 125 combinaciones posibles (las 60 históricas + repetidas)
    struct RotorPerm { int r1, r2, r3; };
    vector<RotorPerm> perms;
    for (int r1 = 0; r1 < 5; ++r1) {
        for (int r2 = 0; r2 < 5; ++r2) {
            if (r2 == r1) continue;
            for (int r3 = 0; r3 < 5; ++r3) {
                if (r3 == r1 || r3 == r2) continue;
                perms.push_back({r1, r2, r3});
            }
        }
    }
    for (int r1 = 0; r1 < 5; ++r1) {
        for (int r2 = 0; r2 < 5; ++r2) {
            for (int r3 = 0; r3 < 5; ++r3) {
                if (r1 == r2 || r2 == r3 || r1 == r3) {
                    perms.push_back({r1, r2, r3});
                }
            }
        }
    }

    struct RotorCandidate {
        int r1, r2, r3;
        int p1, p2, p3;
        double ioc;
    };

    cout << "[Fase 1/2] Escaneando " << (perms.size() * 17576) << " configuraciones de rotores con IoC...\n";

    vector<RotorCandidate> top_candidates;
    const double IOC_THRESHOLD = 0.046;

    #pragma omp parallel
    {
        vector<RotorCandidate> local_cands;

        #pragma omp for schedule(dynamic)
        for (size_t pi = 0; pi < perms.size(); ++pi) {
            int r1 = perms[pi].r1;
            int r2 = perms[pi].r2;
            int r3 = perms[pi].r3;

            for (int p1 = 0; p1 < 26; ++p1) {
                for (int p2 = 0; p2 < 26; ++p2) {
                    for (int p3 = 0; p3 < 26; ++p3) {
                        Enigma sim;
                        sim.configure_indices(r1, r2, r3, 0, 0, 0, p1, p2, p3, reflector_type);

                        int counts[26] = {0};
                        for (uint8_t c : clean_cipher) {
                            counts[sim.process_letter(c)]++;
                        }

                        long long sum = 0;
                        for (int i = 0; i < 26; ++i) sum += (long long)counts[i] * (counts[i] - 1);
                        double ioc = (double)sum / ((double)clean_cipher.size() * (clean_cipher.size() - 1));

                        if (ioc >= IOC_THRESHOLD) {
                            local_cands.push_back({r1, r2, r3, p1, p2, p3, ioc});
                        }
                    }
                }
            }
        }

        #pragma omp critical
        {
            top_candidates.insert(top_candidates.end(), local_cands.begin(), local_cands.end());
        }
    }

    sort(top_candidates.begin(), top_candidates.end(), [](const RotorCandidate& a, const RotorCandidate& b) {
        return a.ioc > b.ioc;
    });

    auto t1 = chrono::high_resolution_clock::now();
    auto ms_phase1 = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();

    cout << "  -> Fase 1 completada en " << ms_phase1 << " ms. Candidatos filtrados: " << top_candidates.size() << "\n\n";

    if (top_candidates.empty()) {
        cout << "[!] No se detectaron picos de IoC suficientes.\n";
        return {};
    }

    int candidates_to_test = min((int)top_candidates.size(), 30);
    cout << "[Fase 2/2] Optimizando clavijero (Steckerbrett) en los " << candidates_to_test << " mejores candidatos...\n";

    const char* rotor_names[5] = {"I", "II", "III", "IV", "V"};
    ModernCrackResult best_result;
    best_result.language_score = -1e12;

    for (int ci = 0; ci < candidates_to_test; ++ci) {
        const auto& cand = top_candidates[ci];

        uint8_t cur_plugs[26];
        for (int i = 0; i < 26; ++i) cur_plugs[i] = i;

        auto eval_plugs = [&](const uint8_t* pl) {
            Enigma sim;
            sim.configure_indices(cand.r1, cand.r2, cand.r3, 0, 0, 0, cand.p1, cand.p2, cand.p3, reflector_type, pl);
            vector<uint8_t> dec(clean_cipher.size());
            for (size_t i = 0; i < clean_cipher.size(); ++i) dec[i] = sim.process_letter(clean_cipher[i]);
            return lm.score(dec);
        };

        double cur_score = eval_plugs(cur_plugs);

        bool improved = true;
        int passes = 0;
        while (improved && passes < 10) {
            improved = false;
            passes++;
            for (int a = 0; a < 26; ++a) {
                for (int b = a + 1; b < 26; ++b) {
                    uint8_t test_plugs[26];
                    memcpy(test_plugs, cur_plugs, 26);

                    int old_a = test_plugs[a];
                    int old_b = test_plugs[b];
                    test_plugs[old_a] = old_a;
                    test_plugs[old_b] = old_b;

                    test_plugs[a] = b;
                    test_plugs[b] = a;

                    double sc = eval_plugs(test_plugs);
                    if (sc > cur_score) {
                        cur_score = sc;
                        memcpy(cur_plugs, test_plugs, 26);
                        improved = true;
                    }
                }
            }
        }

        if (cur_score > best_result.language_score) {
            best_result.language_score = cur_score;
            best_result.rotors = string(rotor_names[cand.r1]) + "-" + rotor_names[cand.r2] + "-" + rotor_names[cand.r3];
            best_result.positions = string(1, (char)('A' + cand.p1)) + (char)('A' + cand.p2) + (char)('A' + cand.p3);
            best_result.ioc = cand.ioc;

            string plugs_str = "";
            for (int i = 0; i < 26; ++i) {
                if (cur_plugs[i] > i) {
                    if (!plugs_str.empty()) plugs_str += " ";
                    plugs_str += (char)('A' + i);
                    plugs_str += (char)('A' + cur_plugs[i]);
                }
            }
            best_result.plugboard = plugs_str.empty() ? "(Ninguno)" : plugs_str;

            Enigma final_sim;
            final_sim.configure_indices(cand.r1, cand.r2, cand.r3, 0, 0, 0, cand.p1, cand.p2, cand.p3, reflector_type, cur_plugs);
            best_result.plaintext = "";
            for (uint8_t c : clean_cipher) {
                best_result.plaintext += (char)('A' + final_sim.process_letter(c));
            }
        }
    }

    auto t2 = chrono::high_resolution_clock::now();
    auto total_ms = chrono::duration_cast<chrono::milliseconds>(t2 - t0).count();

    cout << "[OK] Descifrado completado en: " << total_ms << " ms.\n";
    cout << "=======================================================\n";
    cout << "               RESULTADOS DEL CRIPTANÁLISIS            \n";
    cout << "=======================================================\n";
    cout << "Rotores encontrados   : " << best_result.rotors << "\n";
    cout << "Posición inicial      : " << best_result.positions << "\n";
    cout << "Clavijero descubierto : " << best_result.plugboard << "\n";
    cout << "Indice de coincidencia: " << fixed << setprecision(4) << best_result.ioc << "\n";
    cout << "Puntuación de idioma  : " << best_result.language_score << "\n\n";
    cout << "TEXTO DESCIFRADO:\n" << best_result.plaintext << "\n";
    cout << "=======================================================\n";

    return best_result;
}

// ============================================================================
// ATAQUE POR CRIB (BLETCHLEY PARK / BOMBA DE TURING)
// ============================================================================

struct CrackResult {
    string rotors;
    string positions;
    string decrypted_sample;
    int match_score;
};

void run_crib_attack(const string& ciphertext, const string& crib, char reflector_type = 'B') {
    cout << "\n=======================================================\n";
    cout << "  INICIANDO ATAQUE CRIB (ESTILO BLETCHLEY PARK / BOMBE)\n";
    cout << "=======================================================\n";
    cout << "Texto cifrado : " << ciphertext.substr(0, 40) << (ciphertext.size() > 40 ? "..." : "") << "\n";
    cout << "Crib (clave)  : " << crib << "\n";

    string clean_cipher = "";
    for (char c : ciphertext) if (isalpha(c)) clean_cipher += (char)toupper(c);

    string clean_crib = "";
    for (char c : crib) if (isalpha(c)) clean_crib += (char)toupper(c);

    if (clean_crib.empty() || clean_cipher.size() < clean_crib.size()) {
        cout << "[!] Error: El crib no puede estar vacio o ser mas largo que el texto cifrado.\n";
        return;
    }

    auto t0 = chrono::high_resolution_clock::now();

    vector<CrackResult> matches;
    const char* rotor_names[5] = {"I", "II", "III", "IV", "V"};

    vector<int> valid_offsets;
    for (size_t offset = 0; offset <= clean_cipher.size() - clean_crib.size(); ++offset) {
        bool crash = false;
        for (size_t k = 0; k < clean_crib.size(); ++k) {
            if (clean_crib[k] == clean_cipher[offset + k]) {
                crash = true;
                break;
            }
        }
        if (!crash) valid_offsets.push_back(offset);
    }

    cout << "Posiciones validas para el crib (filtrando colisiones N!=E(N)): " 
         << valid_offsets.size() << "/" << (clean_cipher.size() - clean_crib.size() + 1) << "\n";

    // 125 combinaciones posibles
    struct RotorPerm { int r1, r2, r3; };
    vector<RotorPerm> perms;
    for (int r1 = 0; r1 < 5; ++r1) {
        for (int r2 = 0; r2 < 5; ++r2) {
            for (int r3 = 0; r3 < 5; ++r3) {
                perms.push_back({r1, r2, r3});
            }
        }
    }

    cout << "Probando 125 combinaciones de rotores x 17,576 posiciones = 2,197,000 claves...\n\n";

    #pragma omp parallel for schedule(dynamic)
    for (size_t pi = 0; pi < perms.size(); ++pi) {
        int r1 = perms[pi].r1;
        int r2 = perms[pi].r2;
        int r3 = perms[pi].r3;

        for (int p1 = 0; p1 < 26; ++p1) {
            for (int p2 = 0; p2 < 26; ++p2) {
                for (int p3 = 0; p3 < 26; ++p3) {
                    for (int offset : valid_offsets) {
                        Enigma sim;
                        sim.configure_indices(r1, r2, r3, 0, 0, 0, p1, p2, p3, reflector_type);

                        for (int s = 0; s < offset; ++s) {
                            sim.process_letter(clean_cipher[s] - 'A');
                        }

                        bool match = true;
                        for (size_t k = 0; k < clean_crib.size(); ++k) {
                            uint8_t dec = sim.process_letter(clean_cipher[offset + k] - 'A');
                            if ((char)(dec + 'A') != clean_crib[k]) {
                                match = false;
                                break;
                            }
                        }

                        if (match) {
                            Enigma full_dec;
                            full_dec.configure_indices(r1, r2, r3, 0, 0, 0, p1, p2, p3, reflector_type);
                            string preview = full_dec.process_text(clean_cipher);

                            string r_str = string(rotor_names[r1]) + "-" + rotor_names[r2] + "-" + rotor_names[r3];
                            string p_str = "";
                            p_str += (char)('A' + p1);
                            p_str += (char)('A' + p2);
                            p_str += (char)('A' + p3);

                            #pragma omp critical
                            matches.push_back({r_str, p_str, preview.substr(0, 50), offset});
                        }
                    }
                }
            }
        }
    }

    auto t1 = chrono::high_resolution_clock::now();
    auto ms = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();
    long long tested_keys = (long long)perms.size() * 26 * 26 * 26;

    cout << "\n[OK] Busqueda completada en: " << ms << " ms (" << tested_keys << " claves evaluadas).\n";
    cout << "Coincidencias encontradas: " << matches.size() << "\n\n";

    for (size_t i = 0; i < min(matches.size(), (size_t)10); ++i) {
        cout << "Coincidencia #" << (i + 1) << ":\n";
        cout << "  Rotores          : " << matches[i].rotors << "\n";
        cout << "  Posicion inicial : " << matches[i].positions << "\n";
        cout << "  Offset del crib  : " << matches[i].match_score << "\n";
        cout << "  Texto descifrado : " << matches[i].decrypted_sample << "...\n\n";
    }
}

// ============================================================================
// MENÚ INTERACTIVO CON VALIDACIÓN PASO A PASO
// ============================================================================

void print_help() {
    cout << "=====================================================================\n";
    cout << "   SIMULADOR Y DESCIFRADOR DE ENIGMA I / M3 (C++17)                  \n";
    cout << "=====================================================================\n";
    cout << "Uso: enigma [opciones]\n\n";
    cout << "Modos de Criptanálisis (Descifrado):\n";
    cout << "  --blind                     Descifrado 100% a ciegas (Ciphertext-Only, sin crib)\n";
    cout << "  --lang <es|de|en>           Idioma para descifrado a ciegas (def: es)\n";
    cout << "  --crack <crib>              Ataque por crib (palabra sospechada, Bletchley Park)\n\n";
    cout << "Opciones de Operación Normal (con clave conocida):\n";
    cout << "  -r, --rotors <R1,R2,R3>     Rotores de izq a der (ej: I,II,III o 1,2,3 o I-II-III)\n";
    cout << "  -p, --pos <XYZ>             Posicion inicial (Grundstellung, ej: AAA o A-A-A)\n";
    cout << "  -g, --rings <G1,G2,G3>      Anillos (Ringstellung, ej: 01,01,01 o AAA)\n";
    cout << "  -u, --reflector <B|C>       Reflector (UKW-B o UKW-C, por defecto B)\n";
    cout << "  -s, --plugs <\"AB CD...\">    Clavijero (Steckerbrett, pares separados por espacio)\n";
    cout << "  -t, --text <mensaje>        Texto a procesar\n";
    cout << "  --military                  Formato militar (mayusculas en bloques de 5)\n";
    cout << "  -h, --help                  Muestra esta ayuda\n\n";
}

void run_interactive_menu() {
    const char* rotor_names[5] = {"I", "II", "III", "IV", "V"};

    while (true) {
        cout << "\n=======================================================\n";
        cout << "       SIMULADOR Y DESCIFRADOR DE ENIGMA (C++17)       \n";
        cout << "=======================================================\n";
        cout << "1. Codificar / Decodificar mensaje (con clave conocida)\n";
        cout << "2. Descifrador MODERNO A CIEGAS (sin clave ni crib)\n";
        cout << "3. Descifrador clasico por CRIB (Bletchley Park / Bombe)\n";
        cout << "4. Caso historico real (Operacion Barbarroja 1941)\n";
        cout << "5. Benchmark de rendimiento (>100M caracteres/seg)\n";
        cout << "6. Salir\n";
        cout << "Seleccione una opcion [1-6]: ";

        string opt;
        if (!getline(cin, opt)) break;
        if (opt.empty()) continue;

        if (opt == "1") {
            int r1 = 0, r2 = 1, r3 = 2;
            int p1 = 0, p2 = 0, p3 = 0;
            int g1 = 0, g2 = 0, g3 = 0;
            char ref_c = 'B';
            string plugs = "";
            string text_in = "";
            string err = "";

            // 1. Rotores
            while (true) {
                cout << "\nRotores (ej. I,II,III o 1,2,3 o I-II-III) [I,II,III]: ";
                string r_in;
                getline(cin, r_in);
                if (validate_rotors_input(r_in, r1, r2, r3, err)) break;
                cout << "[!] Error: " << err << "\n";
            }

            // 2. Posición inicial
            while (true) {
                cout << "Posicion inicial (ej. AAA o A-A-A) [AAA]: ";
                string p_in;
                getline(cin, p_in);
                if (validate_positions_input(p_in, p1, p2, p3, err)) break;
                cout << "[!] Error: " << err << "\n";
            }

            // 3. Anillos
            while (true) {
                cout << "Anillos (Ringstellung, ej. 01,01,01 o AAA o 1-1-1) [01,01,01]: ";
                string g_in;
                getline(cin, g_in);
                if (validate_rings_input(g_in, g1, g2, g3, err)) break;
                cout << "[!] Error: " << err << "\n";
            }

            // 4. Reflector
            while (true) {
                cout << "Reflector (B o C) [B]: ";
                string ref_in;
                getline(cin, ref_in);
                if (validate_reflector_input(ref_in, ref_c, err)) break;
                cout << "[!] Error: " << err << "\n";
            }

            // 5. Clavijero
            while (true) {
                cout << "Clavijero / Steckerbrett (ej. AV BS CG o vacio): ";
                string s_in;
                getline(cin, s_in);
                if (validate_plugboard_input(s_in, plugs, err)) break;
                cout << "[!] Error: " << err << "\n";
            }

            // 6. Texto
            cout << "\nTexto a procesar: ";
            getline(cin, text_in);

            // Resumen de configuración confirmada
            cout << "\n=======================================================\n";
            cout << "              CONFIGURACIÓN DE LA MÁQUINA              \n";
            cout << "=======================================================\n";
            cout << "  Rotores configurados : " << rotor_names[r1] << " - " << rotor_names[r2] << " - " << rotor_names[r3];
            if (r1 == r2 || r2 == r3 || r1 == r3) cout << " (Aviso: Rotores repetidos)";
            cout << "\n";
            cout << "  Posición inicial     : " << (char)('A' + p1) << " - " << (char)('A' + p2) << " - " << (char)('A' + p3) << "\n";
            cout << "  Anillos              : " << (g1 + 1) << " - " << (g2 + 1) << " - " << (g3 + 1) << "\n";
            cout << "  Reflector            : UKW-" << ref_c << "\n";
            cout << "  Clavijero            : " << (plugs.empty() ? "(Sin cables)" : plugs) << "\n";
            cout << "=======================================================\n";

            Enigma m;
            m.configure_indices(r1, r2, r3, g1, g2, g3, p1, p2, p3, ref_c);
            if (!plugs.empty()) m.set_plugboard(plugs);

            auto t0 = chrono::high_resolution_clock::now();
            string result = m.process_text(text_in);
            auto t1 = chrono::high_resolution_clock::now();
            auto us = chrono::duration_cast<chrono::microseconds>(t1 - t0).count();

            cout << "\n-------------------------------------------------------\n";
            cout << "Texto procesado: " << result << "\n";
            cout << "Tiempo         : " << us << " microsegundos\n";
            cout << "-------------------------------------------------------\n";

        } else if (opt == "2") {
            string cipher, lang_in;
            cout << "\nIngrese el texto cifrado: ";
            getline(cin, cipher);

            cout << "Idioma presunto (1=Espanol, 2=Aleman, 3=Ingles) [1]: ";
            getline(cin, lang_in);
            Language l = LANG_ES;
            if (lang_in == "2") l = LANG_DE;
            else if (lang_in == "3") l = LANG_EN;

            run_modern_blind_crack(cipher, l);

        } else if (opt == "3") {
            string cipher, crib;
            cout << "\nIngrese el texto cifrado: ";
            getline(cin, cipher);
            cout << "Crib (palabra sospechada, ej: HOLA, MENSAJE, ESTE): ";
            getline(cin, crib);
            run_crib_attack(cipher, crib);

        } else if (opt == "4") {
            cout << "\n--- MENSAJE HISTORICO: OPERACION BARBARROJA (7 JULIO 1941) ---\n";
            string cipher = "EDPUD NRGYS ZRCXN UYTPO MRMBO FKTBZ REZKM LXLVE FGUEY";
            cout << "Cifrado interceptado:\n  " << cipher << "\n\n";

            Enigma m;
            m.configure_indices(1, 3, 4, 1, 20, 11, 'B'-'A', 'L'-'A', 'A'-'A', 'B');
            m.set_plugboard("AV BS CG DL FU HZ IN KM OW RX");

            auto t0 = chrono::high_resolution_clock::now();
            string plain = m.process_text(cipher);
            auto t1 = chrono::high_resolution_clock::now();
            auto us = chrono::duration_cast<chrono::microseconds>(t1 - t0).count();

            cout << "Texto descifrado:\n  " << plain << "\n\n";
            cout << "Tiempo de ejecucion: " << us << " microsegundos.\n";

        } else if (opt == "5") {
            cout << "\nEjecutando benchmark de 10,000,000 caracteres...\n";
            Enigma m;
            m.configure_indices(0, 1, 2, 0, 0, 0, 0, 0, 0, 'B');
            auto t0 = chrono::high_resolution_clock::now();
            uint8_t dummy = 0;
            for (int i = 0; i < 10000000; ++i) {
                dummy ^= m.process_letter(i % 26);
            }
            auto t1 = chrono::high_resolution_clock::now();
            auto ms = chrono::duration_cast<chrono::milliseconds>(t1 - t0).count();
            cout << "Procesados 10,000,000 caracteres en " << ms << " ms.\n";
            cout << "Velocidad: " << fixed << setprecision(2) << (10.0 / (ms / 1000.0)) << " millones de caracteres/segundo.\n";

        } else if (opt == "6") {
            cout << "Saliendo...\n";
            break;
        }
    }
}

// ============================================================================
// MAIN / CLI
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc == 1) {
        run_interactive_menu();
        return 0;
    }

    string rotors_arg = "I,II,III";
    string pos_arg = "AAA";
    string rings_arg = "01,01,01";
    string plugs_arg = "";
    string text_arg = "";
    string crib_arg = "";
    string lang_arg = "es";
    bool blind_crack = false;
    char reflector_arg = 'B';
    bool military_fmt = false;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help();
            return 0;
        } else if (arg == "--blind") {
            blind_crack = true;
        } else if (arg == "--lang" && i + 1 < argc) {
            lang_arg = argv[++i];
        } else if ((arg == "-r" || arg == "--rotors") && i + 1 < argc) {
            rotors_arg = argv[++i];
        } else if ((arg == "-p" || arg == "--pos") && i + 1 < argc) {
            pos_arg = argv[++i];
        } else if ((arg == "-g" || arg == "--rings") && i + 1 < argc) {
            rings_arg = argv[++i];
        } else if ((arg == "-u" || arg == "--reflector") && i + 1 < argc) {
            reflector_arg = argv[++i][0];
        } else if ((arg == "-s" || arg == "--plugs") && i + 1 < argc) {
            plugs_arg = argv[++i];
        } else if ((arg == "-t" || arg == "--text") && i + 1 < argc) {
            text_arg = argv[++i];
        } else if (arg == "--crack" && i + 1 < argc) {
            crib_arg = argv[++i];
        } else if (arg == "--military") {
            military_fmt = true;
        }
    }

    if (text_arg.empty()) {
        string line;
        while (getline(cin, line)) {
            if (!text_arg.empty()) text_arg += "\n";
            text_arg += line;
        }
    }

    if (blind_crack) {
        Language l = LANG_ES;
        if (lang_arg == "de") l = LANG_DE;
        else if (lang_arg == "en") l = LANG_EN;
        run_modern_blind_crack(text_arg, l, reflector_arg);
        return 0;
    }

    if (!crib_arg.empty()) {
        run_crib_attack(text_arg, crib_arg, reflector_arg);
        return 0;
    }

    int r1, r2, r3, p1, p2, p3, g1, g2, g3;
    char ref_c;
    string plugs_clean, err;

    if (!validate_rotors_input(rotors_arg, r1, r2, r3, err)) {
        cerr << "[!] Error en rotores: " << err << "\n"; return 1;
    }
    if (!validate_positions_input(pos_arg, p1, p2, p3, err)) {
        cerr << "[!] Error en posicion: " << err << "\n"; return 1;
    }
    if (!validate_rings_input(rings_arg, g1, g2, g3, err)) {
        cerr << "[!] Error en anillos: " << err << "\n"; return 1;
    }
    if (!validate_reflector_input(string(1, reflector_arg), ref_c, err)) {
        cerr << "[!] Error en reflector: " << err << "\n"; return 1;
    }
    if (!validate_plugboard_input(plugs_arg, plugs_clean, err)) {
        cerr << "[!] Error en clavijero: " << err << "\n"; return 1;
    }

    Enigma machine;
    machine.configure_indices(r1, r2, r3, g1, g2, g3, p1, p2, p3, ref_c);
    if (!plugs_clean.empty()) machine.set_plugboard(plugs_clean);

    string output = machine.process_text(text_arg, !military_fmt);
    cout << output << endl;

    return 0;
}

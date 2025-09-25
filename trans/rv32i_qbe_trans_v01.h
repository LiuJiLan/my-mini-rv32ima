// Created by liujilan on 2025/9/25.
//

#ifndef MY_MINI_RV32IMA_RV32I_QBE_TRANS_V01_H
#define MY_MINI_RV32IMA_RV32I_QBE_TRANS_V01_H

// rv32i_qbe_trans_v01.h
// Minimal streaming RV32I -> QBE translator (v01)

#pragma once
#include <string>
#include <sstream>
#include <optional>
#include <cstdint>

struct MemMapV01 {
    static constexpr uint32_t kRamBase  = 0x80000000u;   // MINIRV32_RAM_IMAGE_OFFSET
    static constexpr uint32_t kRamSize  = (128u << 20);  // 128 MB (按需调整)
    static constexpr uint32_t kRamLimit = kRamBase + kRamSize;
    static inline bool is_ram_addr(uint32_t addr) {
        return (addr >= kRamBase) && (addr < kRamLimit);
    }
};

class Rv32iQbeTrans_v01 {
public:
    void init(const std::string& func_name) {
        reset();
        func_ = func_name;
        // 🔧 关键修正：函数头必须包含返回类型 w；指针参数用 l（64-bit）
        ss_ << "export function w $" << func_ << "(l %state, l %ram, w %pc_in) {\n";
        ss_ << "@L0\n";
        ss_ << "        %pc =w copy %pc_in\n";
    }

    // 返回：若可翻译则给出片段（不含函数收尾）；不可翻译则 nullopt
    std::optional<std::string> translateOne(uint32_t ir, bool allowMem) {
        const uint32_t opc   = ir & 0x7F;
        const int rd         = (ir >> 7)  & 0x1F;
        const int funct3     = (ir >> 12) & 0x7;
        const int rs1        = (ir >> 15) & 0x1F;
        const int rs2        = (ir >> 20) & 0x1F;
        const int funct7     = (ir >> 25) & 0x7F;

        // 读寄存器：x0 恒为 0；其它寄存器用 %xN
        auto R = [&](int x) -> std::string {
            if (x == 0) return "0";
            return "%x" + std::to_string(x);
        };
        // 写寄存器：x0 写入丢弃，输出注释；否则生成 "%xN =w <expr>"
        auto SET = [&](int rdd, const std::string& expr, std::ostringstream& out) {
            if (rdd == 0) {
                out << "        # x0 <- " << expr << " (ignored)\n";
            } else {
                out << "        %x" << rdd << " =w " << expr << "\n";
            }
        };

        auto immI  = [&](){ return (int32_t)ir >> 20; };
        auto immU  = [&](){ return (int32_t)(ir & 0xFFFFF000); };
        auto immS  = [&](){ int32_t v = (int32_t)(((ir >> 25) << 5) | ((ir >> 7) & 0x1F)); return (v << 20) >> 20; };

        std::ostringstream out;

        // 先把跳转/系统类指令交回解释器（v01 只做直线指令）
        if (opc == 0x63 || opc == 0x6F || opc == 0x67 || opc == 0x73 || opc == 0x0F) {
            return std::nullopt;
        }

        // ===== LUI =====
        if (opc == 0x37) {
            SET(rd, std::to_string(immU()), out);
            return out.str();
        }

        // ===== AUIPC =====
        if (opc == 0x17) {
            // pc 已在寄存器 %pc 中，如需要可拓展：%t =w add %pc, immU ; SET(rd, %t)
            // 这里 v01 先交回解释器；如需支持请自行拓展。
            return std::nullopt;
        }

        // ===== I-type LOAD ===== (LB/LH/LW/LBU/LHU)
        if (opc == 0x03) {
            if (!allowMem) return std::nullopt;
            const int32_t imm = immI();
            const std::string addr = newTmp(), ofs = newTmp(), ptr = newTmp(), tmp = newTmp();

            out << "        " << addr << " =w add " << R(rs1) << ", " << imm << "\n";
            out << "        " << ofs  << " =w sub " << addr << ", " << MemMapV01::kRamBase << "\n";
            out << "        " << ptr  << " =l add %ram, " << ofs << "\n";

            switch (funct3) {
                case 0x0: out << "        " << tmp  << " =w loadsb " << ptr << "\n"; SET(rd, tmp, out); return out.str(); // LB
                case 0x1: out << "        " << tmp  << " =w loadsh " << ptr << "\n"; SET(rd, tmp, out); return out.str(); // LH
                case 0x2: out << "        " << tmp  << " =w loadw "  << ptr << "\n"; SET(rd, tmp, out); return out.str(); // LW
                case 0x4: out << "        " << tmp  << " =w loadub " << ptr << "\n"; SET(rd, tmp, out); return out.str(); // LBU
                case 0x5: out << "        " << tmp  << " =w loaduh " << ptr << "\n"; SET(rd, tmp, out); return out.str(); // LHU
                default:  return std::nullopt;
            }
        }

        // ===== S-type STORE ===== (SB/SH/SW)
        if (opc == 0x23) {
            if (!allowMem) return std::nullopt;
            const int32_t simm = immS();
            const std::string addr = newTmp(), ofs = newTmp(), ptr = newTmp();

            out << "        " << addr << " =w add " << R(rs1) << ", " << simm << "\n";
            out << "        " << ofs  << " =w sub " << addr << ", " << MemMapV01::kRamBase << "\n";
            out << "        " << ptr  << " =l add %ram, " << ofs << "\n";

            switch (funct3) {
                case 0x0: out << "        storeb " << R(rs2) << ", " << ptr << "\n"; return out.str(); // SB
                case 0x1: out << "        storeh " << R(rs2) << ", " << ptr << "\n"; return out.str(); // SH
                case 0x2: out << "        storew " << R(rs2) << ", " << ptr << "\n"; return out.str(); // SW
                default:  return std::nullopt;
            }
        }

        // ===== Op-imm / Op（算术逻辑，非乘除） =====
        if (opc == 0x13 || opc == 0x33) {
            const bool is_reg = (opc == 0x33);       // 0x33: reg op, 0x13: imm op
            const int32_t imm = immI();
            const std::string rhs = is_reg ? R(rs2) : std::to_string(imm);

            // RV32M（乘除）在 v01 交回解释器
            if (is_reg && (funct7 & 0x20)) return std::nullopt;

            switch (funct3) {
                case 0x0: { // ADD/SUB or ADDI
                    if (is_reg && (funct7 & 0x20)) return std::nullopt; // SUB 已处理
                    SET(rd, std::string("add ") + R(rs1) + ", " + rhs, out);
                    return out.str();
                }
                case 0x1:  // SLL / SLLI
                    SET(rd, std::string("shl ") + R(rs1) + ", " + rhs, out); return out.str();
                case 0x2:  // SLT / SLTI
                    SET(rd, std::string("cslt ") + R(rs1) + ", " + rhs, out); return out.str();
                case 0x3:  // SLTU / SLTIU
                    SET(rd, std::string("csltu ") + R(rs1) + ", " + rhs, out); return out.str();
                case 0x4:  // XOR / XORI
                    SET(rd, std::string("xor ") + R(rs1) + ", " + rhs, out); return out.str();
                case 0x5:  // SRL/SRA or SRLI/SRAI
                    if (is_reg) {
                        // srl/sra 需根据 funct7 决定：这里 v01 统一用 srl；sra 交回解释器或自行扩展
                        if (funct7 & 0x20) return std::nullopt; // SRA/SRAI 交回
                        SET(rd, std::string("shr ") + R(rs1) + ", " + rhs, out); return out.str();
                    } else {
                        if (funct7 & 0x20) return std::nullopt; // SRAI 交回
                        SET(rd, std::string("shr ") + R(rs1) + ", " + rhs, out); return out.str();
                    }
                case 0x6:  // OR / ORI
                    SET(rd, std::string("or ") + R(rs1) + ", " + rhs, out); return out.str();
                case 0x7:  // AND / ANDI
                    SET(rd, std::string("and ") + R(rs1) + ", " + rhs, out); return out.str();
                default:
                    return std::nullopt;
            }
        }

        // 其它暂不支持
        return std::nullopt;
    }

    void commit(const std::string& piece) {
        if (!piece.empty()) {
            ss_ << piece;
            if (piece.back() != '\n') ss_ << "\n"; // 保障换行，避免下一句粘连
        }
    }

    std::string finalize() {
        ss_ << "        ret %pc_in\n";
        ss_ << "}\n";
        return ss_.str();
    }

private:
    std::string newTmp() { return std::string("%t") + std::to_string(tmpId_++); }
    void reset() { func_.clear(); ss_.str(""); ss_.clear(); tmpId_ = 0; }

private:
    std::string        func_;
    std::ostringstream ss_;
    int                tmpId_ = 0;
};

#endif //MY_MINI_RV32IMA_RV32I_QBE_TRANS_V01_H

/*
 * Copyright (c) 2007
 * Nintendo Co., Ltd.
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appear in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation.  Nintendo makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <new>
#include <es/ent.h>
#include "esidl.h"
#include "string.h"
#include "expr.h"

class TypeOffsetter : public Visitor
{
    size_t offset;

    void processMembers(const Node* node)
    {
        assert(!node->isLeaf());
        for (NodeList::iterator i = node->begin(); i != node->end(); ++i)
        {
            assert(dynamic_cast<Member*>(*i));
            process(static_cast<Member*>(*i), node);
        }
    }

    void process(const Node* node, const Node* scope)
    {
        for (;;)
        {
            if (const ArrayDcl* array = dynamic_cast<const ArrayDcl*>(node))
            {
                if (node->getOffset())
                {
                    return;
                }
                node->setOffset(offset);

                offset += Ent::Array::getSize(array->getDimension());
            }
            if (const Member* member = dynamic_cast<const Member*>(node))
            {
                node = member->getSpec();
                scope = member->getParent();
            }
            if (const ScopedName* scopedName = dynamic_cast<const ScopedName*>(node))
            {
                node = scopedName->search(scope);
                continue;
            }
            break;
        }

        if (node->getOffset())
        {
            return;
        }
        node->setOffset(offset);

        if (const SequenceType* seq = dynamic_cast<const SequenceType*>(node))
        {
            offset += Ent::Sequence::getSize();

            process(seq->getSpec(), scope);
        }
        else if (const ExceptDcl* exc = dynamic_cast<const ExceptDcl*>(node))
        {
            offset += Ent::Exception::getSize(exc->getMemberCount());
            processMembers(exc);
        }
        else if (const StructType* st = dynamic_cast<const StructType*>(node))
        {
            offset += Ent::Structure::getSize(st->getMemberCount());
            processMembers(st);
        }
        else if (const Interface* i = dynamic_cast<const Interface*>(node))
        {
            // Imported interface. Do not process members.
            offset += sizeof(Interface);
        }
    }

public:
    TypeOffsetter(size_t base) :
        offset(base)
    {
    }

    size_t getOffset() const
    {
        return offset;
    }

    virtual void at(const Node* node)
    {
        if (1 < node->getRank())
        {
            return;
        }

        visitChildren(node);
    }

    virtual void at(const Attribute* node)
    {
        if (1 < node->getRank())
        {
            return;
        }

        process(node->getSpec(), node->getParent());
    }

    virtual void at(const OpDcl* node)
    {
        process(node->getSpec(), node->getParent());
        visitChildren(node);
        if (Node* raises = node->getRaises())
        {
            for (NodeList::iterator i = raises->begin(); i != raises->end(); ++i)
            {
                process(*i, node->getParent());
            }
        }
    }

    virtual void at(const ParamDcl* node)
    {
        process(node, node->getParent());
    }
};

class EntOffsetter : public Visitor
{
    size_t offset;

public:
    EntOffsetter() :
        offset(sizeof(Ent::Header))
    {
    }

    size_t getOffset() const
    {
        return offset;
    }

    virtual void at(const Module* node)
    {
        if (1 < node->getRank())
        {
            return;
        }

        node->setOffset(offset);
        offset += Ent::Module::getSize(node->getModuleCount(), node->getInterfaceCount(), node->getConstCount());

        visitChildren(node);
    }

    virtual void at(const EnumType* node)
    {
        if (1 < node->getRank())
        {
            return;
        }

        node->setOffset(offset);
        offset += Ent::Enum::getSize(node->getMemberCount());
    }

    virtual void at(const Interface* node)
    {
        if (1 < node->getRank())
        {
            return;
        }

        if (node->isLeaf()) // forward decl.
        {
            return;
        }

        node->setOffset(offset);
        offset += Ent::Interface::getSize(node->getMethodCount(), node->getConstCount());

        visitChildren(node);
    }

    virtual void at(const Attribute* node)
    {
        if (1 < node->getRank())
        {
            return;
        }

        node->setOffset(offset);
        offset += Ent::Method::getSize(0, 0);
        if (!node->isReadonly())
        {
            offset += Ent::Method::getSize(1, 0);
        }
    }

    virtual void at(const OpDcl* node)
    {
        if (1 < node->getRank())
        {
            return;
        }

        node->setOffset(offset);
        offset += Ent::Method::getSize(node->getParamCount(), node->getRaiseCount());
    }
};

class Emitter : public Visitor
{
    static const char* specTable[];

    std::map<std::string, size_t>& dict;
    u8* image;
    size_t fileSize;

    Ent::Spec getSpec(const std::string& name)
    {
        for (int i = 0; i < Ent::MaxSpec; ++i)
        {
            if (name.compare(specTable[i]) == 0)
            {
                return Ent::SpecPrimitive | i;
            }
        }

        fprintf(stderr, "Inv. type '%s'\n", name.c_str());
        exit(EXIT_FAILURE);

        return 0;
    }

    Ent::Spec getSpec(const Node* node, const Node* scope)
    {
        for (;;)
        {
            if (const ArrayDcl* array = dynamic_cast<const ArrayDcl*>(node))
            {
                return node->getOffset();
            }
            if (const Member* member = dynamic_cast<const Member*>(node))
            {
                node = member->getSpec();
                scope = member->getParent();
            }
            if (const ScopedName* scopedName = dynamic_cast<const ScopedName*>(node))
            {
                node = scopedName->search(scope);
                continue;
            }
            break;
        }

        if (const Type* type = dynamic_cast<const Type*>(node))
        {
            return getSpec(type->getName());
        }

        assert(node->getOffset());
        return node->getOffset();
    }

public:
    Emitter(std::map<std::string, size_t>& dict, size_t fileSize) :
        dict(dict),
        image(0),
        fileSize(fileSize)
    {
        assert(sizeof(Ent::Header) < fileSize);
        image = new u8[fileSize];
        memset(image, 0, fileSize);

        new(image) Ent::Header(fileSize);

        for (std::map<std::string, size_t>::iterator i = dict.begin();
             i != dict.end();
             ++i)
        {
            strcpy(reinterpret_cast<char*>(image) + i->second, i->first.c_str());
            printf("%04llx: \"%s\"\n", i->second, i->first.c_str());
        }
    }

    ~Emitter()
    {
        if (image)
        {
            delete[] image;
        }
    }

    void emit(FILE* file)
    {
        fwrite(image, fileSize, 1, file);
    }

    virtual void at(const Module* node)
    {
        if (node->getOffset() == 0)
        {
            return;
        }

        printf("%04llx: Module %s\n", node->getOffset(), node->getName().c_str());

        Node* parent = node->getParent();
        if (parent)
        {
            assert(dynamic_cast<Module*>(parent));
            Ent::Module* module = reinterpret_cast<Ent::Module*>(image + parent->getOffset());
            module->addModule(node->getOffset());
        }

        new(image + node->getOffset()) Ent::Module(dict[node->getName()], parent ? parent->getOffset() : 0,
                                                   node->getModuleCount(), node->getInterfaceCount(), node->getConstCount());


        size_t offset = node->getOffset() + sizeof(Ent::Module) + sizeof(Ent::Spec) * (node->getModuleCount() + node->getInterfaceCount());
        for (NodeList::iterator i = node->begin(); i != node->end(); ++i)
        {
            if (ConstDcl* c = dynamic_cast<ConstDcl*>(*i))
            {
                c->setOffset(offset);
                offset += sizeof(Ent::Constant);
            }
        }

        visitChildren(node);
    }

    virtual void at(const EnumType* node)
    {
        if (node->getOffset() == 0)
        {
            return;
        }

        printf("%04llx: Enum %s\n", node->getOffset(), node->getName().c_str());

        Ent::Enum* e = new(image + node->getOffset()) Ent::Enum(node->getMemberCount());
        for (NodeList::iterator i = node->begin(); i != node->end(); ++i)
        {
            e->add(dict[(*i)->getName()]);
        }
    }

    virtual void at(const StructType* node)
    {
        if (node->getOffset() == 0)
        {
            return;
        }

        printf("%04llx: Structure %s\n", node->getOffset(), node->getName().c_str());

        Ent::Structure* st = new(image + node->getOffset()) Ent::Structure(node->getMemberCount());
        for (NodeList::iterator i = node->begin(); i != node->end(); ++i)
        {
            assert(dynamic_cast<Member*>(*i));
            Member* member = static_cast<Member*>(*i);
            st->addMember(getSpec(member, node), dict[member->getName()]);

            printf("    Member %s : %x\n", member->getName().c_str(), getSpec(member, node));
        }

        visitChildren(node);
    }

    virtual void at(const ExceptDcl* node)
    {
        if (node->getOffset() == 0)
        {
            return;
        }

        printf("%04llx: Exception %s\n", node->getOffset(), node->getName().c_str());

        Ent::Exception* exc = new(image + node->getOffset()) Ent::Exception(node->getMemberCount());
        for (NodeList::iterator i = node->begin(); i != node->end(); ++i)
        {
            assert(dynamic_cast<Member*>(*i));
            Member* member = static_cast<Member*>(*i);
            exc->addMember(getSpec(member, node), dict[member->getName()]);

            printf("    Member %s : %x\n", member->getName().c_str(), getSpec(member, node));
        }

        visitChildren(node);
    }

    virtual void at(const Interface* node)
    {
        if (node->getOffset() == 0)
        {
            return;
        }

        printf("%04llx: Interface %s\n", node->getOffset(), node->getName().c_str());

        Node* parent = node->getParent();
        assert(parent);
        assert(dynamic_cast<Module*>(parent));

        if (node->getRank() == 1)
        {
            Ent::Module* module = reinterpret_cast<Ent::Module*>(image + parent->getOffset());
            module->addInterface(node->getOffset());
        }

        Guid iid;
        node->getIID(iid);

        Guid piid = GUID_NULL;
        u32 inheritedMethodCount = 0;
        if (Node* extends = node->getExtends())
        {
            for (NodeList::iterator i = extends->begin(); i != extends->end(); ++i)
            {
                ScopedName* scoped = static_cast<ScopedName*>(*i);
                Node* base = scoped->search(node);
                Interface* super = static_cast<Interface*>(base);
                super->getIID(piid);
                inheritedMethodCount += super->getMethodCount();
                while (Node* extends = super->getExtends())
                {
                    for (NodeList::iterator i = extends->begin(); i != extends->end(); ++i)
                    {
                        scoped = static_cast<ScopedName*>(*i);
                        base = scoped->search(node);
                        super = static_cast<Interface*>(base);
                        inheritedMethodCount += super->getMethodCount();
                        break;  // Multiple inheritance is not allowed.
                    }
                }
                break;  // Multiple inheritance is not allowed.
            }
        }

        if (1 < node->getRank())
        {
            new(image + node->getOffset()) Ent::Interface(0, iid, piid, 0,
                                                          node->getMethodCount(), node->getConstCount(), inheritedMethodCount);
            return;
        }

        new(image + node->getOffset()) Ent::Interface(dict[node->getName()], iid, piid, parent->getOffset(),
                                                      node->getMethodCount(), node->getConstCount(), inheritedMethodCount);

        size_t offset = node->getOffset() + sizeof(Ent::Interface) + sizeof(Ent::Spec) * node->getMethodCount();
        for (NodeList::iterator i = node->begin(); i != node->end(); ++i)
        {
            if (ConstDcl* c = dynamic_cast<ConstDcl*>(*i))
            {
                c->setOffset(offset);
                offset += sizeof(Ent::Constant);
            }
        }

        visitChildren(node);
    }

    virtual void at(const SequenceType* node)
    {
        if (node->getOffset() == 0)
        {
            return;
        }

        Ent::Spec spec = getSpec(node->getSpec(), getCurrent());

        printf("%04llx: Sequence<%x>\n", node->getOffset(), spec);

        if (Node* max = node->getMax())
        {
            EvalInteger<u64> eval(node->getParent());
            max->accept(&eval);
            new(image + node->getOffset()) Ent::Sequence(spec, eval.getValue());
        }
        else
        {
            new(image + node->getOffset()) Ent::Sequence(spec);
        }
    }

    virtual void at(const Member* node)
    {
        node->getSpec()->accept(this);
    }

    virtual void at(const ArrayDcl* node)
    {
        if (node->getOffset() == 0)
        {
            return;
        }

        Ent::Spec spec = getSpec(node->getSpec(), getCurrent());

        printf("%04llx: Array of %x - %s\n", node->getOffset(), spec, node->getName().c_str());

        Ent::Array* array = new(image + node->getOffset()) Ent::Array(spec, node->getDimension());
        for (NodeList::iterator i = node->begin(); i != node->end(); ++i)
        {
            EvalInteger<u32> eval(node->getParent());
            (*i)->accept(&eval);
            array->setRank(eval.getValue());
        }

        node->getSpec()->accept(this);
    }

    virtual void at(const Attribute* node)
    {
        if (node->getOffset() == 0)
        {
            return;
        }

        Node* parent = node->getParent();
        assert(parent);
        assert(dynamic_cast<Interface*>(parent));
        Ent::Interface* interface = reinterpret_cast<Ent::Interface*>(image + parent->getOffset());
        assert(interface->type == Ent::TypeInterface);

        interface->addMethod(node->getOffset());
        Ent::Spec spec = getSpec(node->getSpec(), getCurrent());
        printf("%04llx: Getter %s : %x\n", node->getOffset(), node->getName().c_str(), spec);
        new(image + node->getOffset()) Ent::Method(spec, dict[node->getName()], Ent::Method::AttrGetter, 0, 0);
        if (!node->isReadonly())
        {
            interface->addMethod(node->getOffset() + Ent::Method::getSize(0, 0));
            printf("%04llx: Setter %s : %x\n", node->getOffset() + Ent::Method::getSize(0, 0), node->getName().c_str(), spec);
            Ent::Method* setter = new(image + node->getOffset() + Ent::Method::getSize(0, 0))
                                    Ent::Method(Ent::SpecVoid, dict[node->getName()], Ent::Method::AttrSetter, 1, 0);
            setter->addParam(spec, dict[node->getName()], Ent::Param::AttrIn);
        }

        node->getSpec()->accept(this);
    }

    virtual void at(const ConstDcl* node)
    {
        if (node->getOffset() == 0)
        {
            return;
        }

        Type* type = node->getType();
        assert(type);
        Ent::Spec spec = getSpec(type->getName());

        printf("%04llx: Constant %s %x\n", node->getOffset(), node->getName().c_str(), spec);

        switch (spec)
        {
        case Ent::SpecS16:
            {
                EvalInteger<s16> eval(node->getParent());
                node->getExp()->accept(&eval);
                new(image + node->getOffset()) Ent::Constant(spec, dict[node->getName()], static_cast<u32>(eval.getValue()));
            }
            break;
        case Ent::SpecS32:
            {
                EvalInteger<s32> eval(node->getParent());
                node->getExp()->accept(&eval);
                new(image + node->getOffset()) Ent::Constant(spec, dict[node->getName()], static_cast<u32>(eval.getValue()));
            }
            break;
        case Ent::SpecS64:
            {
                EvalInteger<s64> eval(node->getParent());
                node->getExp()->accept(&eval);
                new(image + node->getOffset()) Ent::Constant(spec, dict[node->getName()], node->getValue());
                *reinterpret_cast<s64*>(image + node->getValue()) = eval.getValue();
            }
            break;
        case Ent::SpecU8:
            {
                EvalInteger<u8> eval(node->getParent());
                node->getExp()->accept(&eval);
                new(image + node->getOffset()) Ent::Constant(spec, dict[node->getName()], static_cast<u32>(eval.getValue()));
            }
            break;
        case Ent::SpecU16:
            {
                EvalInteger<u16> eval(node->getParent());
                node->getExp()->accept(&eval);
                new(image + node->getOffset()) Ent::Constant(spec, dict[node->getName()], static_cast<u32>(eval.getValue()));
            }
            break;
        case Ent::SpecU32:
            {
                EvalInteger<u32> eval(node->getParent());
                node->getExp()->accept(&eval);
                new(image + node->getOffset()) Ent::Constant(spec, dict[node->getName()], static_cast<u32>(eval.getValue()));
            }
            break;
        case Ent::SpecU64:
            {
                EvalInteger<u64> eval(node->getParent());
                node->getExp()->accept(&eval);
                new(image + node->getOffset()) Ent::Constant(spec, dict[node->getName()], node->getValue());
                *reinterpret_cast<u64*>(image + node->getValue()) = eval.getValue();
            }
            break;
        case Ent::SpecBool:
            {
                EvalInteger<bool> eval(node->getParent());
                node->getExp()->accept(&eval);
                new(image + node->getOffset()) Ent::Constant(spec, dict[node->getName()], static_cast<u32>(eval.getValue()));
            }
            break;
        case Ent::SpecF32:
            {
                EvalFloat<float> eval(node->getParent());
                node->getExp()->accept(&eval);
                float value = eval.getValue();
                new(image + node->getOffset()) Ent::Constant(spec, dict[node->getName()], *reinterpret_cast<u32*>(&value));
            }
            break;
        case Ent::SpecF64:
            {
                EvalFloat<double> eval(node->getParent());
                node->getExp()->accept(&eval);
                double value = eval.getValue();
                new(image + node->getOffset()) Ent::Constant(spec, dict[node->getName()], node->getValue());
                *reinterpret_cast<double*>(image + node->getValue()) = eval.getValue();
            }
            break;
        case Ent::SpecF128:
            {
                EvalFloat<long double> eval(node->getParent());
                node->getExp()->accept(&eval);
                long double value = eval.getValue();
                new(image + node->getOffset()) Ent::Constant(spec, dict[node->getName()], node->getValue());
                *reinterpret_cast<long double*>(image + node->getValue()) = eval.getValue();
            }
            break;
        case Ent::SpecChar:
            {
                EvalString<char> eval(node->getParent());
                node->getExp()->accept(&eval);
                new(image + node->getOffset()) Ent::Constant(spec, dict[node->getName()], static_cast<u32>(eval.getValue()));
            }
            break;
        case Ent::SpecWChar:
            {
                EvalString<wchar_t> eval(node->getParent());
                node->getExp()->accept(&eval);
                new(image + node->getOffset()) Ent::Constant(spec, dict[node->getName()], static_cast<u32>(eval.getValue()));
            }
            break;
        case Ent::SpecString:
            {
                EvalString<std::string> eval(node->getParent());
                node->getExp()->accept(&eval);
                new(image + node->getOffset()) Ent::Constant(spec, dict[node->getName()], node->getValue());
                strcpy(reinterpret_cast<char*>(image + node->getValue()), eval.getValue().c_str());
            }
            break;
        case Ent::SpecWString:
            {
                EvalString<std::string> eval(node->getParent());
                node->getExp()->accept(&eval);
                new(image + node->getOffset()) Ent::Constant(spec, dict[node->getName()], node->getValue());
                strcpy(reinterpret_cast<char*>(image + node->getValue()), eval.getValue().c_str());
            }
            break;
        default:
            fprintf(stderr, "Inv. const type.\n");
            exit(EXIT_FAILURE);
            break;
        }
    }

    virtual void at(const OpDcl* node)
    {
        Node* parent = node->getParent();
        assert(parent);
        assert(dynamic_cast<Interface*>(parent));
        Ent::Interface* interface = reinterpret_cast<Ent::Interface*>(image + parent->getOffset());
        assert(interface->type == Ent::TypeInterface);
        interface->addMethod(node->getOffset());

        Ent::Spec spec = getSpec(node->getSpec(), getCurrent());
        Ent::Method* method = new(image + node->getOffset()) Ent::Method(spec, dict[node->getName()], 0,
                                                                         node->getParamCount(), node->getRaiseCount());

        printf("%04llx: Method %s : %x\n", node->getOffset(), node->getName().c_str(), spec);
        for (NodeList::iterator i = node->begin(); i != node->end(); ++i)
        {
            assert(dynamic_cast<ParamDcl*>(*i));
            ParamDcl* param = static_cast<ParamDcl*>(*i);
            Ent::Spec spec = getSpec(param->getSpec(), node);
            method->addParam(spec, dict[param->getName()], param->getAttr());

            printf("  Param %s : %x\n", param->getName().c_str(), spec);
        }

        if (Node* raises = node->getRaises())
        {
            for (NodeList::iterator i = raises->begin(); i != raises->end(); ++i)
            {
                method->addRaise(getSpec(*i, node));
                printf("  Raise %x\n", getSpec(*i, node));
            }
        }

        node->getSpec()->accept(this);
    }
};

const char* Emitter::specTable[] =
{
    "",                     // SpecS8
    "short",                // SpecS16
    "long",                 // SpecS32
    "long long",            // SpecS64
    "octet",                // SpecU8
    "unsigned short",       // SpecU16
    "unsigned long",        // SpecU32
    "unsigned long long",   // SpecU64
    "float",                // SpecF32
    "double",               // SpecF64
    "long double",          // SpecF128
    "boolean",              // SpecBool
    "char",                 // SpecChar
    "wchar",                // SpecWChar
    "void",                 // SpecVoid
    "uuid",                 // SpecUuid
    "string",               // SpecString
    "wstring",              // SpecWString
    "any",                  // SpecAny
    "Object",               // SpecObject
    "fixed",                // SpecFixed
    "ValueBase",            // SpecValue
};

void printEnt(const std::string& filename)
{
    printf("# %s\n", filename.c_str());

    size_t offset = 0;

    EntOffsetter eo;
    getSpecification()->accept(&eo);
    offset = eo.getOffset();

    std::map<std::string, size_t> dict;
    StringOffsetter so(dict, offset);
    getSpecification()->accept(&so);
    offset = so.getOffset();

    offset += 3u;
    offset &= ~3u;

    ConstOffsetter co(offset);
    getSpecification()->accept(&co);
    offset = co.getOffset();

    offset += 3u;
    offset &= ~3u;

    TypeOffsetter to(offset);
    getSpecification()->accept(&to);
    offset = to.getOffset();

    // Emit
    printf("-----------------------------------\n");

    Emitter emitter(dict, offset);
    getSpecification()->accept(&emitter);

    FILE* file = fopen(filename.c_str(), "wb");
    if (file)
    {
        emitter.emit(file);
        fclose(file);
    }
}
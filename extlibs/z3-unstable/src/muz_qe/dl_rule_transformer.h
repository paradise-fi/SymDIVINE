/*++
Copyright (c) 2006 Microsoft Corporation

Module Name:

    dl_rule_transformer.h

Abstract:

    <abstract>

Author:

    Krystof Hoder (t-khoder) 2010-09-20.

Revision History:

--*/
#ifndef _DL_RULE_TRANSFORMER_H_
#define _DL_RULE_TRANSFORMER_H_

#include"map.h"
#include"vector.h"
#include"dl_rule.h"
#include"dl_rule_set.h"
#include"model_converter.h"
#include"proof_converter.h"

namespace datalog {

    class rule_transformer {
    public:
        class plugin;
    private:
        friend class plugin;

        typedef svector<plugin*> plugin_vector;
        struct plugin_comparator;

        context & m_context;
        rule_manager & m_rule_manager;
        bool m_dirty;
        svector<plugin*> m_plugins;
        
        void ensure_ordered();
    public:

        rule_transformer(context & ctx);
        ~rule_transformer();
        
        /**
           \brief Add a plugin for rule transformation.

           The rule_transformer object takes ownership of the plugin object.
        */
        void register_plugin(plugin * p);

        /**
           \brief Transform the rule set using the registered transformation plugins. If the rule 
           set has changed, return true; otherwise return false.
        */
        bool operator()(rule_set & rules, model_converter_ref& mc, proof_converter_ref& pc);
    };
    
    class rule_transformer::plugin {
        friend class rule_transformer;

        unsigned m_priority;
        bool m_can_destratify_negation;
        rule_transformer * m_transformer;

        void attach(rule_transformer & transformer) { m_transformer = &transformer; }

    protected:
        /**
           \brief Create a plugin object for rule_transformer.

           The priority argument determines in what order will rules be applied to the rules
           (higher priority plugins will be applied first).
        */
        plugin(unsigned priority, bool can_destratify_negation = false) : m_priority(priority), 
            m_can_destratify_negation(can_destratify_negation), m_transformer(0) {}
    public:
        virtual ~plugin() {}

        unsigned get_priority() { return m_priority; }
        bool can_destratify_negation() const { return m_can_destratify_negation; }

        /**
           \brief Return \c rule_set object with containing transformed rules or 0 if no
           transformation was done.

           The caller takes ownership of the returned \c rule_set object.
        */
        virtual rule_set * operator()(rule_set const & source,
                                      model_converter_ref& mc,
                                      proof_converter_ref& pc) = 0;

        /**
        Removes duplicate tails.
        */
        static void remove_duplicate_tails(app_ref_vector& tail, svector<bool>& tail_neg);

    };
};

#endif /* _DL_RULE_TRANSFORMER_H_ */


// (c) 2004 Mark Kretschmann <markey@web.de>
// See COPYING file for licensing information.

#ifndef AMAROK_SCRIPTMANAGER_H
#define AMAROK_SCRIPTMANAGER_H

#include "scriptmanager_selector.h"

#include <qobject.h>        //baseclass
#include <qstringlist.h>    //stack allocated

namespace KJSEmbed
{
    class KJSEmbedPart;
}

namespace ScriptManager
{
    
    class Manager : public QObject
    {
            Q_OBJECT

        public:
            Manager( QObject* );
            ~ Manager();

            static void showSelector();
            void addObject( QObject* object );

        private:
            static Manager* self;

            KJSEmbed::KJSEmbedPart* m_kjs;
            QStringList m_list;
    };

} //namespace ScriptManager


#endif /* AMAROK_SCRIPTMANAGER_H */



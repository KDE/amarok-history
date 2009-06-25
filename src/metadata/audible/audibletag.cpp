/***************************************************************************
    copyright            : (C) 2005 by Martin Aumueller
    email                : aumuell@reserv.at

    copyright            : (C) 2005 by Andy Leadbetter
    email                : andrew.leadbetter@gmail.com
                           (original mp4 implementation)
 ***************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it  under the terms of the GNU Lesser General Public License version  *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,            *
 *   MA  02110-1301  USA                                                   *
 ***************************************************************************/

#include "audibletag.h"

#include <taglib/tag.h>

#include <netinet/in.h> // ntohl
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace TagLib;

Audible::Tag::Tag() : TagLib::Tag::Tag() {
    m_title = String::null;
    m_artist = String::null;
    m_album = String::null;
    m_comment = String::null;
    m_genre = String::null;
    m_year = 0;
    m_track = 0;
    m_userID = 0;
    m_tagsEndOffset = -1;
}

Audible::Tag::~Tag() {
}

bool Audible::Tag::isEmpty() const {
    return  m_title == String::null &&
        m_artist == String::null &&
        m_album == String::null && 
        m_comment == String::null &&
        m_genre == String::null &&
        m_year == 0 &&
        m_track == 0 &&
        m_userID == 0;
}

void Audible::Tag::duplicate(const Tag *source, Tag *target, bool overwrite) {
    // No nonstandard information stored yet
    Tag::duplicate(source, target, overwrite);
}

#define OFF_PRODUCT_ID 197
#define OFF_TAGS 189

void Audible::Tag::readTags( FILE *fp )
{
    char buf[1023];
    fseek(fp, OFF_PRODUCT_ID, SEEK_SET);
    if (fread(buf, strlen("product_id"), 1, fp) != 1)
        return;
    if(memcmp(buf, "product_id", strlen("product_id")))
    {
        buf[20]='\0';
        fprintf(stderr, "no valid Audible aa file: %s\n", buf);
        return;
    }

    // Now parse tag.

    fseek(fp, OFF_TAGS, SEEK_SET);
    char *name, *value;

    m_tagsEndOffset = OFF_TAGS;

    bool lasttag = false;
    while(!lasttag)
    {
        lasttag = !readTag(fp, &name, &value);
        if(!strcmp(name, "title"))
        {
            m_title = String(value, String::Latin1);
        }
        else if(!strcmp(name, "author"))
        {
            m_artist = String(value, String::Latin1);
        }
        else if(!strcmp(name, "long_description"))
        {
            m_comment = String(value, String::Latin1);
        }
        else if(!strcmp(name, "description"))
        {
            if( m_comment.isNull() )
                m_comment = String(value, String::Latin1);
        }
        else if(!strcmp(name, "pubdate"))
        {
            m_year = 0;
            char *p = strrchr(value, '-');
            if(p)
                m_year = strtol(p+1, NULL, 10);
        }
        else if(!strcmp(name, "user_id"))
        {
            m_userID = strtol(value, NULL, 10);
        }

        delete[] name;
        delete[] value;
    }

    m_album  =  String("", String::Latin1);
    m_track = 0;
    m_genre = String("Audiobook", String::Latin1);
}

bool Audible::Tag::readTag( FILE *fp, char **name, char **value)
{
    // arbitrary value that has to be smaller than 2^32-1 and that should be large enough for all tags                                                                                         
    const uint32_t maxtaglen = 100000;    

    uint32_t nlen;
    if (fread(&nlen, sizeof(nlen), 1, fp) != 1)
        return false;
    nlen = ntohl(nlen);
    //fprintf(stderr, "tagname len=%x\n", (unsigned)nlen);
    if (nlen > maxtaglen)
        return false;

    uint32_t vlen;
    if (fread(&vlen, sizeof(vlen), 1, fp) != 1)
        return false;
    vlen = ntohl(vlen);
    //fprintf(stderr, "tag len=%x\n", (unsigned)vlen);
    if (vlen > maxtaglen)
        return false;

    *name = new char[nlen+1];
    if (!*name)
        return false;
        
    *value = new char[vlen+1];
    if (!*value)
    {
        delete[] *name;
        *name = 0;
        return false;
    }

    (*name)[nlen] = '\0';
    (*value)[vlen] = '\0';

    if (fread(*name, nlen, 1, fp) != 1)
    {
        delete[] *name;
        *name = 0;
        delete[] *value;
        *value = 0;
        return false;
    }
    if (fread(*value, vlen, 1, fp) != 1)
    {
        delete[] *name;
        *name = 0;
        delete[] *value;
        *value = 0;
        return false;
    }
    char lasttag;
    if (fread(&lasttag, 1, 1, fp) != 1)
    {
        delete[] *name;
        *name = 0;
        delete[] *value;
        *value = 0;
        return false;
    }
    //fprintf(stderr, "%s: \"%s\"\n", *name, *value);

    m_tagsEndOffset += 2 * 4 + nlen + vlen + 1;

    return !lasttag;
}

int Audible::Tag::getTagsEndOffset()
{
    return m_tagsEndOffset;
}

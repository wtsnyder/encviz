/**
 * \file
 * \brief XML Parsing
 *
 * Utility functions for parsing through XML documents.
 */

#include <stdexcept>
#include <encviz/xml_config.h>

namespace encviz
{

/**
 * Get Node Text with Checking
 *
 * \param[in] node Element with text
 * \return Tag text
 */
const char *xml_text(tinyxml2::XMLElement *node)
{
    if (node == nullptr)
    {
        throw std::runtime_error("Cannot get text from null element");
    }
    const char *ptr = node->GetText();
    if (ptr == nullptr)
    {
        throw std::runtime_error("Tag " + std::string(node->Name()) + " may not be empty");
    }
    return ptr;
}

/**
 * Query all child nodes of type
 *
 * \param[in] root XML root node
 * \param[in] name XML tag name
 * \return All matching nodes
 */
std::vector<tinyxml2::XMLElement*> xml_query_all(tinyxml2::XMLElement *root,
                                                 const char *name)
{
    std::vector<tinyxml2::XMLElement*> nodes;

    for (tinyxml2::XMLElement *node = root->FirstChildElement(name);
         node != nullptr;
         node = node->NextSiblingElement(name))
    {
        nodes.push_back(node);
    }

    return nodes;
}

/**
 * Query unique child node
 *
 * \param[in] root XML root node
 * \param[in] name XML tag name
 * \return Returned node
 */
tinyxml2::XMLElement *xml_query(tinyxml2::XMLElement *root,
                                const char *name)
{
    std::vector<tinyxml2::XMLElement*> nodes = xml_query_all(root, name);

    if (nodes.empty())
    {
        throw std::runtime_error("Tag " + std::string(name) + " not found in " + std::string(root->Value()) + " on line " + std::to_string(root->GetLineNum()));
    }
    else if (nodes.size() > 1)
    {
        throw std::runtime_error("Tag " + std::string(name) + " must be unique");
    }

    return nodes[0];
}

}; // ~namespace encviz

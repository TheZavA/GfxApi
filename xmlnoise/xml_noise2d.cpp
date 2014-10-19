#include "xml_noise2d.hpp"

#include "xml_noise_decls.hpp"


xml_noise2d_t::xml_noise2d_t(noisepp::Pipeline2D& pipeline)
    : pipeline(pipeline)
    , root(NULL)
{

}

xml_noise2d_t::~xml_noise2d_t()
{
}

//Simple recursive version, for testing. Broken ATM.
void xml_noise2d_t::simple_load(const std::string& xml_file_name)
{
    
    using namespace tinyxml2;
    


    tinyxml2::XMLDocument doc;
    doc.LoadFile(xml_file_name.c_str());
    
    XMLElement* root_noise_element = doc.FirstChildElement("noise");
    if (!root_noise_element)
        //TODO: inherit from 
        throw xml_noise_error_t("no 'noise' element at the root");

    XMLElement* current = root_noise_element->FirstChildElement();

    
    if (!current)
        //TODO: inherit from 
        throw xml_noise_error_t("'noise' element must have one and only one child");
    
    root = simple_traverse(*current);
    
}


module_ptr_t xml_noise2d_t::simple_traverse(const tinyxml2::XMLElement& element)
{
    using namespace noisepp;
    using namespace tinyxml2;
    
    child_modules_t child_modules;
    
    const XMLElement* child_ptr = element.FirstChildElement();
    while (child_ptr)
    {
        const tinyxml2::XMLElement& child = *child_ptr;
        
        module_ptr_t child_module_ptr = simple_traverse(child);
        
        child_modules.push_back(child_module_ptr);
        
        child_ptr = child_ptr->NextSiblingElement();
    }
    
    
    
    //Visit current
    {

        std::string name = element.Name();

        
        handlers2d_t::const_iterator w = handlers.find(name);

        //if the handler name isn't found
        if (w == handlers.end()) 
        {
            throw xml_noise_unknown_handler_error_t(name);
        }


        const tag_handler2d_t& handler = w->second;

        //Call the handler, which will return a noise++ module as a result.
        module_ptr_t module_ptr = handler(pipeline, element, child_modules);

        //Save this on the ptr_list, so it gets owned, and eventually deleted.
        module_ptrs.push_back(module_ptr);

        
        const char* id = element.Attribute("id");


        //If the element has an id attribute
        if (id)
        {
            if (special_nodes.count(id))
                throw xml_noise_duplicate_id_error_t(name,id);

            //Let the user of this class be able to retrieve this element via its unique id.
            special_nodes[id] = module_ptr;
        }
        
        
        
    }
    
    
}



void xml_noise2d_t::load(const std::string& xml_file_name)
{
    using namespace tinyxml2;
    


    tinyxml2::XMLDocument doc;
    doc.LoadFile(xml_file_name.c_str());

    //We will traverse the tree using "post-order depth first search/traversal", with a nice clever stack-free DFS algorithm.
    // Why post order DFS? Because post order DFS ensures that we only visit a node once we already visited all it's children.
    // This means when we create the noisepp module for an xml element, we already have the child modules
    // ready! (if there are any), because we already visited all its children.

    //We will have two cursors, one for the "current" node, and one for the "previous" node.
    // This will allow us to know if we are "headed" down, sideways, or up the tree.
    // DFS will only visit a node when there is nothing below it, or all the nodes below
    // it have already been visited. Using the previous pointer, we can differentiate between
    // visiting a node for the first time on the way down, or on the way up, *after having visited
    // all the children*.

    XMLElement* root_noise_element = doc.FirstChildElement("noise");

    if (!root_noise_element)
        //TODO: inherit from 
        throw xml_noise_error_t("no 'noise' element at the root");

    XMLElement* current = root_noise_element->FirstChildElement();
    XMLElement* previous = NULL;
    
    
    if (!current)
        //TODO: inherit from 
        throw xml_noise_error_t("'noise' element must have one and only one child");
    
    
    while (true) {
        //sanity check
        assert(current);
        
        if (current == root_noise_element)
            break;
        
        //If the previous ptr is null, then we are at the first step, a special case.
        if (!previous)
        {
            //Get the first child element, move down to it.
            XMLElement* child = current->FirstChildElement();

            if (child)
            {
                //If there is a child,

                //Move down to child, and set the vars appropriately.
                previous = current;
                current = child;

                continue;
            } else {
                //There is no child, this tree only has the root, visit the root.
                visit(*current);
                break;
            }
        }
        
        //sanity check
        assert(previous);

        XMLElement* child = current->FirstChildElement();
        XMLElement* next_sibling = current->NextSiblingElement();
        XMLNode* parent_node = current->Parent();
        XMLElement* parent = NULL;

        if (parent_node)
            parent = parent_node->ToElement();

        bool heading_down = (current->Parent() == previous);
        bool heading_up = (previous->Parent() == current);
        bool heading_right = (previous->NextSiblingElement() == current);

        //logical xor all 3 directions; we can only be headed in one direction.
        // bool != bool is equivalent to logical xor.
        // logical xor is also associative, so it can be "nested" like this. I think.
        assert((heading_down != heading_up) != heading_right);

        bool has_child = !!(child);
        bool has_next_sibling = !!(next_sibling);
        bool has_parent = !!(parent);

        bool move_down = !heading_up && has_child;
        bool move_right = (heading_up || !has_child) && has_next_sibling;
        bool move_up = has_parent
             && (      (heading_up && !has_next_sibling)
                    || (heading_right && !has_child && !has_next_sibling)
                    || (heading_down && !has_child && !has_next_sibling) );

        /*
        std::cerr << std::endl << std::endl
            << "------" << std::endl;
        std::cerr << "heading_up: " << heading_up << std::endl;
        std::cerr << "heading_down: " << heading_down << std::endl;
        std::cerr << "heading_right: " << heading_right << std::endl;
        std::cerr << "has_child: " << has_child << std::endl;
        std::cerr << "has_next_sibling: " << has_next_sibling << std::endl;
        std::cerr << "has_parent: " << has_parent << std::endl;
        
        
        
        std::cerr << "move_down: " << move_down << std::endl;
        std::cerr << "move_right: " << move_right << std::endl;
        std::cerr << "move_up: " << move_up << std::endl;
        
        
        std::cerr << "(heading_up && !has_next_sibling): " << (heading_up && !has_next_sibling) << std::endl;
        std::cerr << "(!heading_up && !has_child && !has_next_sibling): "
            << (!heading_up && !has_child && !has_next_sibling) << std::endl;
        std::cerr
            << "------"
            << std::endl << std::endl
            << std::endl;
        */
        
        //Another way of writing this (so we double checking with assertion):
        //assert( move_up == has_parent && !move_down && !move_right );
        //Another way of writing this (so we double checking with assertion):
        assert( move_up == (has_parent
             && (      (heading_up && !has_next_sibling)
                    || (!heading_up && !has_child && !has_next_sibling) ) ));

        bool move_done = !has_parent
             && (      (heading_up && !has_next_sibling)
                    || (!has_child && !has_next_sibling) );

        //logical xor all four moves; we can only move one direction.
        // bool != bool is equivalent to logical xor.
        // logical xor is also associative, so it can be "nested" like this. I think.
        assert( ((move_down != move_right) != move_up) != move_done);

        bool visit_current = (move_right || move_up || move_done);

        if (visit_current)
            visit(*current);

        if (move_done) {
            break;
        } else if (move_down) {
            assert(child);

            previous = current;
            current = child;

            continue;
        } else if (move_right) {
            assert(next_sibling);

            previous = current;
            current = next_sibling;

            continue;
        } else if (move_up) {
            assert(parent);

            previous = current;
            current = parent;

            continue;
        }

    }

    //After all this traversal, we should be left with 1 and only 1 argument/module on the
    // argument stack.
    assert(argument_stack.size() == 1);
    root = argument_stack.top();

    argument_stack.pop();

    //WARNING: you must add root to pipeline yourself outside this class.
    // This is because I am unsure if I must add every element to the pipeline, or just root;
    // for now I add every element, but eventually, I might not need to take the pipeline into
    // account at all, and let the user of the class just add the root as needed.
}


void xml_noise2d_t::visit(const tinyxml2::XMLElement& element)
{
    using namespace tinyxml2;
    using namespace noisepp;

    std::string name = element.Name();

    
    handlers2d_t::const_iterator w = handlers.find(name);

    //if the handler name isn't found
    if (w == handlers.end()) {
        throw xml_noise_unknown_handler_error_t(name);
    }



    std::size_t child_count = 0;

    //Count children
    {
        const XMLElement* child = element.FirstChildElement();
        while(child){ child = child->NextSiblingElement(); ++child_count; }
    }
    
    //There should be this many finished modules on the stack
    assert( child_count <= argument_stack.size());

    //We setup the arguments for the handler.
    child_modules_t arguments(child_count, NULL);

    //Take the arguments off the stack.
    for (std::size_t i = 0; i < child_count; ++i)
    {
        //this is backwards, for brevity, we will reverse it later.
        arguments[i] = argument_stack.top();
        argument_stack.pop();
    }


    //We store them off in reverse order: last first. We need to get them in the correct order.
    std::reverse(arguments.begin(),arguments.end());
    
    const tag_handler2d_t& handler = w->second;

    //Call the handler, which will return a noise++ module as a result.
    module_ptr_t module_ptr = handler(pipeline, element, arguments);

    //Save this on the ptr_list, so it gets owned, and eventually deleted.
    module_ptrs.push_back(module_ptr);

    //Now push this result back on the argument stack, for use by its parent.
    argument_stack.push(module_ptr);

    
    const char* id = element.Attribute("id");


    //If the element has an id attribute
    if (id)
    {
        if (special_nodes.count(id))
            throw xml_noise_duplicate_id_error_t(name,id);

        //Let the user of this class be able to retrieve this element via its unique id.
        special_nodes[id] = module_ptr;
    }
}

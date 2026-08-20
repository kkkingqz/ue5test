#include "GV2ContentEditor/Testing/EditorAdapterConformance.h"

#include <iostream>
#include <string>

int main()
{
    const std::string Error = GV2ContentEditor::Testing::RunEditorAdapterConformance();
    if (!Error.empty())
    {
        std::cerr << "content_editor_conformance=all status=failed error=" << Error << '\n';
        return 1;
    }
    std::cout << "content_editor_conformance=all status=ok suites=4\n";
    return 0;
}

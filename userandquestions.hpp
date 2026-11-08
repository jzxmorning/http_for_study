#pragma once
#include <cctype>
#include <iostream>
#include <boost/algorithm/string.hpp>
#include <string>
#include <vector>
// id、账号、密码、角色身份，以及其它信息
//  typedef enum
//  {
//    student,
//    teacher
//  }Identity;
#include <string>
class User;
inline std::string ReadTextUntilStar()
{
  std::string line, text;
  while (std::getline(std::cin, line))
  {
    if (line == "*")
    {
      break;
    }
    text += line + "\n";
  }
  return text;
}
// std::vector<std::string> u;
// SplitString(str,&u,'\n'); 白雨溪\n542513430201\n25-2\n  jiaozi
// 姓名：
// 学号：
// 班级：
void SplitString(std::string &str, std::vector<std::string> *toke, std::string sep)
{
  boost::split((*toke), str, boost::is_any_of(sep), boost::algorithm::token_compress_on);
}
class User
{
public:
  int id = 0;            // 索引
  std::string name;      // 昵称
  std::string idcount;   // 账号
  std::string passwd;    // 密码
  std::string _identity; // 身份
  int grade = 0;
};

bool IsTeacher(const User &user)
{
  return user._identity == "teacher" || user._identity == "教师" || user._identity == "老师";
}

bool IsStudent(const User &user)
{
  return user._identity == "student" || user._identity == "学生";
}

// 选择题，id，题目，4个选项，标准答案，分值
class Questions
{
public:
  int id = 0;
  std::string title;
  std::string choice;
  std::string answer;
  std::string grade;
  static void ToUpper(std::string &str)
  {
    for (size_t i = 0; i < str.size(); ++i)
    {
      str[i] = (char)std::toupper((unsigned char)str[i]);
    }
  }
  static std::string NormalizeAnswer(std::string str)
  {
    const auto first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
      return "";
    str = str.substr(first, str.find_last_not_of(" \t\r\n") - first + 1);
    ToUpper(str);
    return str;
  }
  bool Match(const std::string &op) const
  {
    const std::string expected = NormalizeAnswer(answer);
    return !expected.empty() && NormalizeAnswer(op) == expected;
  }
};

class QuestionProgress
{
public:
  int question_id = 0;   // 题目编号
  int attempt_count = 0; // 提交次数
  int correct_count = 0; // 答对次数
  std::string last_answer;   // 最近一次的答案
  bool last_correct = false; // 最近一次是否答对
  bool dirty = false;           // 是否存在未保存的修改
  std::uint64_t version = 0;    // 修改次数，用于判断保存期间是否又更新
   std::time_t last_answer_at = 0;        // 最近作答时间
};